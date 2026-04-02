#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <iostream>
#include <vector>
#include <fstream>
#include "nvEncodeAPI.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "nvEncodeAPI.lib")
using Microsoft::WRL::ComPtr;
using namespace std;
static const char* NvEncErrToStr(NVENCSTATUS s) {
    switch (s) {
    case NV_ENC_SUCCESS: return "NV_ENC_SUCCESS";
    case NV_ENC_ERR_NO_ENCODE_DEVICE: return "NV_ENC_ERR_NO_ENCODE_DEVICE";
    case NV_ENC_ERR_UNSUPPORTED_DEVICE: return "NV_ENC_ERR_UNSUPPORTED_DEVICE";
    case NV_ENC_ERR_INVALID_ENCODERDEVICE: return "NV_ENC_ERR_INVALID_ENCODERDEVICE";
    case NV_ENC_ERR_INVALID_DEVICE: return "NV_ENC_ERR_INVALID_DEVICE";
    case NV_ENC_ERR_DEVICE_NOT_EXIST: return "NV_ENC_ERR_DEVICE_NOT_EXIST";
    case NV_ENC_ERR_INVALID_PTR: return "NV_ENC_ERR_INVALID_PTR";
    case NV_ENC_ERR_INVALID_EVENT: return "NV_ENC_ERR_INVALID_EVENT";
    case NV_ENC_ERR_INVALID_PARAM: return "NV_ENC_ERR_INVALID_PARAM";
    case NV_ENC_ERR_INVALID_CALL: return "NV_ENC_ERR_INVALID_CALL";
    case NV_ENC_ERR_OUT_OF_MEMORY: return "NV_ENC_ERR_OUT_OF_MEMORY";
    case NV_ENC_ERR_ENCODER_NOT_INITIALIZED: return "NV_ENC_ERR_ENCODER_NOT_INITIALIZED";
    case NV_ENC_ERR_UNSUPPORTED_PARAM: return "NV_ENC_ERR_UNSUPPORTED_PARAM";
    case NV_ENC_ERR_NEED_MORE_INPUT: return "NV_ENC_ERR_NEED_MORE_INPUT (Not an error, just buffering)";
    default: return "NV_ENC_ERR_UNKNOWN";
    }
}

#define CHECK_NV(st, msg) \
    if ((st) != NV_ENC_SUCCESS) { \
        cout << msg << " failed: " << NvEncErrToStr(st) << endl; \
        return -1; \
    }

int main() {
    HRESULT hr;

    // ------------------------------------------------------------
    // 1) Create D3D11 device with VIDEO_SUPPORT
    // ------------------------------------------------------------
    ComPtr<IDXGIFactory1> factory;
    CreateDXGIFactory1(__uuidof(IDXGIFactory1),
        reinterpret_cast<void**>(factory.GetAddressOf()));

    ComPtr<IDXGIAdapter1> adapter;
    factory->EnumAdapters1(0, adapter.GetAddressOf());

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

    D3D11CreateDevice(
        adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        flags,
        nullptr, 0,
        D3D11_SDK_VERSION,
        device.GetAddressOf(),
        nullptr,
        context.GetAddressOf());
    cout << "D3D11 device created\n";

    // ------------------------------------------------------------
    // 2) Create valid NV12 texture (192x192, above min resolution)
    // ------------------------------------------------------------
    const uint32_t W = 192;
    const uint32_t H = 192;
    D3D11_TEXTURE2D_DESC nv12Desc{};

    nv12Desc.Width = W;
    nv12Desc.Height = H;
    nv12Desc.MipLevels = 1;
    nv12Desc.ArraySize = 1;
    nv12Desc.Format = DXGI_FORMAT_NV12;
    nv12Desc.SampleDesc.Count = 1;
    nv12Desc.Usage = D3D11_USAGE_DEFAULT;
    nv12Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    ComPtr<ID3D11Texture2D> nv12Tex;
    device->CreateTexture2D(&nv12Desc, nullptr, nv12Tex.GetAddressOf());

    // Fill Y plane
    vector<uint8_t> y(W * H, 128);
    vector<uint8_t> uv((W / 2) * (H / 2) * 2, 128);
    D3D11_BOX box{};

    box.left = 0; box.top = 0; box.right = W; box.bottom = H; box.back = 1;
    context->UpdateSubresource(nv12Tex.Get(), 0, &box, y.data(), W, 0); 
    box.right = W / 2; box.bottom = H / 2; box.front = 0; box.back = 1;
    context->UpdateSubresource(nv12Tex.Get(), 1, &box, uv.data(), W, 0);
    cout << "NV12 texture ready\n";

    // ------------------------------------------------------------
    // 3) Load NVENC API
    // ------------------------------------------------------------

    NV_ENCODE_API_FUNCTION_LIST api{};
    api.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    CHECK_NV(NvEncodeAPICreateInstance(&api), "NvEncodeAPICreateInstance");

    // ------------------------------------------------------------
    // 4) Open encode session
    // ------------------------------------------------------------
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS open{};
    open.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    open.device = device.Get();
    open.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    open.apiVersion = NVENCAPI_VERSION;
    void* encoder = nullptr;
    CHECK_NV(api.nvEncOpenEncodeSessionEx(&open, &encoder),
        "nvEncOpenEncodeSessionEx");

    // ------------------------------------------------------------
    // 5) Get preset config
    // ------------------------------------------------------------
    NV_ENC_PRESET_CONFIG preset{};
    preset.version = NV_ENC_PRESET_CONFIG_VER;
    preset.presetCfg.version = NV_ENC_CONFIG_VER;
    CHECK_NV(api.nvEncGetEncodePresetConfigEx(
        encoder,
        NV_ENC_CODEC_H264_GUID,
        NV_ENC_PRESET_P4_GUID,
        NV_ENC_TUNING_INFO_LOW_LATENCY,
        &preset),
        "nvEncGetEncodePresetConfigEx");

    // ------------------------------------------------------------
    // 6) Initialize encoder
    // ------------------------------------------------------------
    NV_ENC_CONFIG encCfg = preset.presetCfg;
    encCfg.version = NV_ENC_CONFIG_VER;
    
    encCfg.gopLength = 30;
    encCfg.frameIntervalP = 1;  
    encCfg.rcParams.lookaheadDepth = 0;          
    encCfg.rcParams.enableLookahead = 0;         
    encCfg.rcParams.zeroReorderDelay = 1;

    encCfg.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;
    encCfg.rcParams.averageBitRate = 5000000;
    encCfg.rcParams.maxBitRate = 6000000;
    encCfg.rcParams.vbvBufferSize = 6000000;

    NV_ENC_INITIALIZE_PARAMS init{};
    init.version = NV_ENC_INITIALIZE_PARAMS_VER;
    init.encodeGUID = NV_ENC_CODEC_H264_GUID;
    init.presetGUID = NV_ENC_PRESET_P4_GUID;
    init.encodeWidth = W;
    init.encodeHeight = H;
    init.darWidth = W;
    init.darHeight = H;
    init.frameRateNum = 30;
    init.frameRateDen = 1;
    init.enablePTD = 1;
    init.tuningInfo = NV_ENC_TUNING_INFO_LOW_LATENCY;
    init.encodeConfig = &encCfg;

    CHECK_NV(api.nvEncInitializeEncoder(encoder, &init),
        "nvEncInitializeEncoder");
    cout << "NVENC initialized\n";

    // ------------------------------------------------------------
    // Retrieve sequence parameters (SPS/PPS)
    // ------------------------------------------------------------
    NV_ENC_SEQUENCE_PARAM_PAYLOAD seqParams{};
    seqParams.version = NV_ENC_SEQUENCE_PARAM_PAYLOAD_VER;

    uint8_t spsPpsData[1024] = {0};
    seqParams.spsppsBuffer = spsPpsData;
    seqParams.inBufferSize = sizeof(spsPpsData);

    uint32_t spsPpsSize = 0;
    seqParams.outSPSPPSPayloadSize = &spsPpsSize;
    CHECK_NV(api.nvEncGetSequenceParams(encoder, &seqParams), "nvEncGetSequenceParams");
    cout << "Sequence params retrieved (" << spsPpsSize << " bytes)\n";

    // ------------------------------------------------------------
    // 7) Register NV12 texture
    // ------------------------------------------------------------
    NV_ENC_REGISTER_RESOURCE reg{};
    reg.version = NV_ENC_REGISTER_RESOURCE_VER;
    reg.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
    reg.resourceToRegister = nv12Tex.Get();
    reg.width = W;
    reg.height = H;
    reg.bufferFormat = NV_ENC_BUFFER_FORMAT_NV12;
    CHECK_NV(api.nvEncRegisterResource(encoder, &reg),
        "nvEncRegisterResource");

    // ------------------------------------------------------------
    // 8) Map input
    // ------------------------------------------------------------
    NV_ENC_MAP_INPUT_RESOURCE map{};

    map.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
    map.registeredResource = reg.registeredResource;
    CHECK_NV(api.nvEncMapInputResource(encoder, &map),
        "nvEncMapInputResource");

    // ------------------------------------------------------------
    // 9) Output bitstream
    // ------------------------------------------------------------
    NV_ENC_CREATE_BITSTREAM_BUFFER bit{};
    bit.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;

    CHECK_NV(api.nvEncCreateBitstreamBuffer(encoder, &bit),
        "nvEncCreateBitstreamBuffer");

    NV_ENC_PIC_PARAMS pic{};
    pic.version = NV_ENC_PIC_PARAMS_VER;
    pic.inputBuffer = map.mappedResource;
    pic.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12;
    pic.inputWidth = W;
    pic.inputHeight = H;
    pic.inputPitch = W; 
    pic.outputBitstream = bit.bitstreamBuffer;
    pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    pic.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;
    pic.completionEvent = nullptr;

    CHECK_NV(api.nvEncEncodePicture(encoder, &pic),
        "nvEncEncodePicture");
    api.nvEncUnmapInputResource(encoder, &map);

    // ------------------------------------------------------------
    // 10) Lock and write output (frame data only, headers separate)
    // ------------------------------------------------------------
    NV_ENC_LOCK_BITSTREAM lock{};
    lock.version = NV_ENC_LOCK_BITSTREAM_VER;
    lock.outputBitstream = bit.bitstreamBuffer;

    CHECK_NV(api.nvEncLockBitstream(encoder, &lock),
        "nvEncLockBitstream");

    ofstream ofs("out.h264", ios::binary);
    ofs.write((char*)spsPpsData, spsPpsSize);
    ofs.write((char*)lock.bitstreamBufferPtr, lock.bitstreamSizeInBytes);
    ofs.close();

    cout << "Encoded H.264 written to out.h264 (header: " << spsPpsSize
         << " bytes + frame: " << lock.bitstreamSizeInBytes << " bytes)\n";
    api.nvEncUnlockBitstream(encoder, bit.bitstreamBuffer);

    // ------------------------------------------------------------
    // Cleanup
    // ------------------------------------------------------------
    api.nvEncUnregisterResource(encoder, reg.registeredResource); 
    api.nvEncDestroyBitstreamBuffer(encoder, bit.bitstreamBuffer);
    api.nvEncDestroyEncoder(encoder);

    cout << "DONE\n";
    
    return 0;
}