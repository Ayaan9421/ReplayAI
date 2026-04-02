#include "NVEncoder.h"

#include <cstring>
#include <iostream>

using namespace std;

static const char* NvEncErrToStr(NVENCSTATUS s) {
    switch (s) {
        case NV_ENC_SUCCESS:
            return "NV_ENC_SUCCESS";
        case NV_ENC_ERR_NO_ENCODE_DEVICE:
            return "NV_ENC_ERR_NO_ENCODE_DEVICE";
        case NV_ENC_ERR_UNSUPPORTED_DEVICE:
            return "NV_ENC_ERR_UNSUPPORTED_DEVICE";
        case NV_ENC_ERR_INVALID_ENCODERDEVICE:
            return "NV_ENC_ERR_INVALID_ENCODERDEVICE";
        case NV_ENC_ERR_INVALID_DEVICE:
            return "NV_ENC_ERR_INVALID_DEVICE";
        case NV_ENC_ERR_DEVICE_NOT_EXIST:
            return "NV_ENC_ERR_DEVICE_NOT_EXIST";
        case NV_ENC_ERR_INVALID_PTR:
            return "NV_ENC_ERR_INVALID_PTR";
        case NV_ENC_ERR_INVALID_EVENT:
            return "NV_ENC_ERR_INVALID_EVENT";
        case NV_ENC_ERR_INVALID_PARAM:
            return "NV_ENC_ERR_INVALID_PARAM";
        case NV_ENC_ERR_INVALID_CALL:
            return "NV_ENC_ERR_INVALID_CALL";
        case NV_ENC_ERR_OUT_OF_MEMORY:
            return "NV_ENC_ERR_OUT_OF_MEMORY";
        case NV_ENC_ERR_ENCODER_NOT_INITIALIZED:
            return "NV_ENC_ERR_ENCODER_NOT_INITIALIZED";
        case NV_ENC_ERR_UNSUPPORTED_PARAM:
            return "NV_ENC_ERR_UNSUPPORTED_PARAM";
        case NV_ENC_ERR_NOT_ENOUGH_BUFFER:
            return "NV_ENC_ERR_NOT_ENOUGH_BUFFER just buffering";
        default:
            return "NV_ENC_ERR_UNKNOWN";
    }
}

#define NV_CHECK(x)                                           \
    if ((x) != NV_ENC_SUCCESS) {                              \
        cout << "[NVENC] Error " << NvEncErrToStr(x) << endl; \
        return false;                                         \
    }

NVEncoder::NVEncoder() = default;
NVEncoder::~NVEncoder() { shutdown(); }

bool NVEncoder::initialize(ID3D11Device* device, uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    if (!initNVENC(device)) return false;
    if (!createEncoder(width, height)) return false;

    cout << "[NVEncoder] Initialized Successfully (BGRA -> NVENC) " << endl;
    return true;
}

bool NVEncoder::initNVENC(ID3D11Device* device) {
    m_api.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    NV_CHECK(NvEncodeAPICreateInstance(&m_api));

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS open{};
    open.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    open.device = device;
    open.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    open.apiVersion = NVENCAPI_VERSION;

    NV_CHECK(m_api.nvEncOpenEncodeSessionEx(&open, &m_encoder));
    return true;
}

bool NVEncoder::createEncoder(uint32_t width, uint32_t height) {
    NV_ENC_PRESET_CONFIG preset{};
    preset.version = NV_ENC_PRESET_CONFIG_VER;
    preset.presetCfg.version = NV_ENC_CONFIG_VER;

    NV_CHECK(m_api.nvEncGetEncodePresetConfigEx(m_encoder, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_P7_GUID,
                                                NV_ENC_TUNING_INFO_HIGH_QUALITY, &preset));  // #

    // Safe copy to persistent member config + overrides

    m_encConfig = preset.presetCfg;
    m_encConfig.version = NV_ENC_CONFIG_VER;

    // #
    // m_encConfig.rcParams.enableLookahead = 0;  // Disable look-ahead to avoid buffering/delays
    // m_encConfig.rcParams.lookaheadDepth = 0;   // Explicit (if supported)

    m_encConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;
    // Bitrate settings (adjust based on resolution/framerate)
    // For 1080p60: 8-15 Mbps average, 20-30 Mbps max
    // For 1440p60: 15-25 Mbps average, 40 Mbps max
    // For 4K60: 25-50 Mbps average, 80-100 Mbps max
    // These are for high-quality recording; increase for near-lossless
    m_encConfig.rcParams.averageBitRate = 20000000;  // 20 Mbps example (tune up for higher res)
    m_encConfig.rcParams.maxBitRate = 40000000;      // 40 Mbps peak
    m_encConfig.rcParams.vbvBufferSize = 40000000;   // Match max for smooth VBR

    // Optional: Enable AQ for better detail in complex areas
    m_encConfig.rcParams.enableAQ = 1;          // Spatial AQ
    m_encConfig.rcParams.enableTemporalAQ = 1;  // If supported
    // #

    NV_ENC_INITIALIZE_PARAMS init{};
    init.version = NV_ENC_INITIALIZE_PARAMS_VER;
    init.encodeGUID = NV_ENC_CODEC_H264_GUID;
    init.presetGUID = NV_ENC_PRESET_P7_GUID;
    init.tuningInfo = NV_ENC_TUNING_INFO_HIGH_QUALITY;
    init.encodeWidth = width;
    init.encodeHeight = height;
    init.darWidth = width;
    init.darHeight = height;
    init.frameRateNum = 60;
    init.frameRateDen = 1;
    init.enablePTD = 1;
    init.encodeConfig = &m_encConfig;

    NV_CHECK(m_api.nvEncInitializeEncoder(m_encoder, &init));

    NV_ENC_CREATE_BITSTREAM_BUFFER bs{};
    bs.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
    NV_CHECK(m_api.nvEncCreateBitstreamBuffer(m_encoder, &bs));
    m_bitstreamBuffer = bs.bitstreamBuffer;

    return true;
}

bool NVEncoder::encodeFrame(ID3D11Texture2D* inputTexture, vector<uint8_t>& outBitstream) {
    NV_ENC_REGISTER_RESOURCE reg{};
    reg.version = NV_ENC_REGISTER_RESOURCE_VER;
    reg.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
    reg.resourceToRegister = inputTexture;
    reg.width = m_width;
    reg.height = m_height;
    // WGC → DXGI_FORMAT_B8G8R8A8_UNORM
    // NVENC naming: ABGR is the correct mapping
    reg.bufferFormat = NV_ENC_BUFFER_FORMAT_ABGR;

    NV_CHECK(m_api.nvEncRegisterResource(m_encoder, &reg));
    m_registeredResource = reg.registeredResource;

    NV_ENC_MAP_INPUT_RESOURCE map{};
    map.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
    map.registeredResource = m_registeredResource;

    NV_CHECK(m_api.nvEncMapInputResource(m_encoder, &map));
    m_inputBuffer = map.mappedResource;

    NV_ENC_PIC_PARAMS pic{};
    pic.version = NV_ENC_PIC_PARAMS_VER;
    pic.inputBuffer = m_inputBuffer;
    pic.inputWidth = m_width;
    pic.inputHeight = m_height;
    pic.inputPitch = m_width;
    pic.bufferFmt = reg.bufferFormat;
    pic.outputBitstream = m_bitstreamBuffer;
    pic.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    pic.encodePicFlags = NV_ENC_PIC_FLAG_OUTPUT_SPSPPS | NV_ENC_PIC_FLAG_FORCEIDR;

    NV_CHECK(m_api.nvEncEncodePicture(m_encoder, &pic));

    NV_ENC_LOCK_BITSTREAM lock{};
    lock.version = NV_ENC_LOCK_BITSTREAM_VER;
    lock.outputBitstream = m_bitstreamBuffer;

    NV_CHECK(m_api.nvEncLockBitstream(m_encoder, &lock));

    outBitstream.assign((uint8_t*)lock.bitstreamBufferPtr,
                        (uint8_t*)lock.bitstreamBufferPtr + lock.bitstreamSizeInBytes);

    m_api.nvEncUnlockBitstream(m_encoder, m_bitstreamBuffer);
    m_api.nvEncUnmapInputResource(m_encoder, m_inputBuffer);
    m_api.nvEncUnregisterResource(m_encoder, m_registeredResource);

    return true;
}

void NVEncoder::shutdown() {
    if (m_encoder) {
        m_api.nvEncDestroyEncoder(m_encoder);
        m_encoder = nullptr;
    }
}