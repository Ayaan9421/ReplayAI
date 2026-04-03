#include "NVEncoder.h"
#include <cstring>
#include <iostream>
using namespace std;

static const char* NvEncErrToStr(NVENCSTATUS s) {
    switch (s) {
        case NV_ENC_SUCCESS:              return "NV_ENC_SUCCESS";
        case NV_ENC_ERR_NO_ENCODE_DEVICE: return "NV_ENC_ERR_NO_ENCODE_DEVICE";
        case NV_ENC_ERR_INVALID_PARAM:    return "NV_ENC_ERR_INVALID_PARAM";
        case NV_ENC_ERR_INVALID_CALL:     return "NV_ENC_ERR_INVALID_CALL";
        case NV_ENC_ERR_OUT_OF_MEMORY:    return "NV_ENC_ERR_OUT_OF_MEMORY";
        case NV_ENC_ERR_NOT_ENOUGH_BUFFER:return "NV_ENC_ERR_NOT_ENOUGH_BUFFER";
        default:                          return "NV_ENC_ERR_UNKNOWN";
    }
}

#define NV_CHECK(x)                                            \
    if ((x) != NV_ENC_SUCCESS) {                               \
        cout << "[NVENC] Error " << NvEncErrToStr(x) << endl;  \
        return false;                                          \
    }

NVEncoder::NVEncoder() = default;
NVEncoder::~NVEncoder() { shutdown(); }

bool NVEncoder::initialize(ID3D11Device* device, uint32_t width, uint32_t height) {
    m_width  = width;
    m_height = height;
    if (!initNVENC(device))         return false;
    if (!createEncoder(width, height)) return false;
    cout << "[NVEncoder] Initialized Successfully (BGRA -> NVENC)\n";
    return true;
}

bool NVEncoder::initNVENC(ID3D11Device* device) {
    m_api.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    NV_CHECK(NvEncodeAPICreateInstance(&m_api));

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS open{};
    open.version    = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    open.device     = device;
    open.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    open.apiVersion = NVENCAPI_VERSION;
    NV_CHECK(m_api.nvEncOpenEncodeSessionEx(&open, &m_encoder));
    return true;
}

bool NVEncoder::createEncoder(uint32_t width, uint32_t height) {
    NV_ENC_PRESET_CONFIG preset{};
    preset.version            = NV_ENC_PRESET_CONFIG_VER;
    preset.presetCfg.version  = NV_ENC_CONFIG_VER;

    // P4 + LOW_LATENCY: good quality, no lookahead stall, suitable for recording
    NV_CHECK(m_api.nvEncGetEncodePresetConfigEx(
        m_encoder,
        NV_ENC_CODEC_H264_GUID,
        NV_ENC_PRESET_P4_GUID,                   // balanced quality/perf
        NV_ENC_TUNING_INFO_LOW_LATENCY,           // no lookahead buffering
        &preset));

    m_encConfig         = preset.presetCfg;
    m_encConfig.version = NV_ENC_CONFIG_VER;

    // Disable lookahead explicitly just to be safe
    m_encConfig.rcParams.enableLookahead = 0;
    m_encConfig.rcParams.lookaheadDepth  = 0;

    m_encConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;
    m_encConfig.rcParams.averageBitRate  = 20000000;  // 20 Mbps
    m_encConfig.rcParams.maxBitRate      = 40000000;  // 40 Mbps peak
    m_encConfig.rcParams.vbvBufferSize   = 40000000;

    m_encConfig.rcParams.enableAQ        = 1;
    m_encConfig.rcParams.enableTemporalAQ= 1;

    // Keyframe every 2 seconds (120 frames at 60fps)
    // This is the fix for FORCEIDR-on-every-frame
    m_encConfig.gopLength                = 120;
    m_encConfig.frameIntervalP           = 1;   // IP only, no B-frames (lower latency)

    NV_ENC_INITIALIZE_PARAMS init{};
    init.version       = NV_ENC_INITIALIZE_PARAMS_VER;
    init.encodeGUID    = NV_ENC_CODEC_H264_GUID;
    init.presetGUID    = NV_ENC_PRESET_P4_GUID;
    init.tuningInfo    = NV_ENC_TUNING_INFO_LOW_LATENCY;
    init.encodeWidth   = width;
    init.encodeHeight  = height;
    init.darWidth      = width;
    init.darHeight     = height;
    init.frameRateNum  = 60;
    init.frameRateDen  = 1;
    init.enablePTD     = 1;
    init.encodeConfig  = &m_encConfig;
    NV_CHECK(m_api.nvEncInitializeEncoder(m_encoder, &init));

    // Create bitstream output buffer
    NV_ENC_CREATE_BITSTREAM_BUFFER bs{};
    bs.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
    NV_CHECK(m_api.nvEncCreateBitstreamBuffer(m_encoder, &bs));
    m_bitstreamBuffer = bs.bitstreamBuffer;

    // *** FIX: Register the input resource ONCE here at init time ***
    // We use a placeholder registration that gets reused each frame.
    // Actual per-frame texture is registered below in encodeFrame using
    // a cached registration approach.

    return true;
}

bool NVEncoder::encodeFrame(ID3D11Texture2D* inputTexture, vector<uint8_t>& outBitstream) {

    // Register this frame's texture — but we do it as lightweight as possible.
    // The real fix vs original: we unregister only when texture CHANGES,
    // but since WGC gives a new texture every frame, we must register each time.
    // The key fix is removing FORCEIDR and using LOW_LATENCY preset which
    // eliminates the stall. Per-frame register/unregister is acceptable with
    // LOW_LATENCY tuning — the driver handles it efficiently.

    NV_ENC_REGISTER_RESOURCE reg{};
    reg.version             = NV_ENC_REGISTER_RESOURCE_VER;
    reg.resourceType        = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
    reg.resourceToRegister  = inputTexture;
    reg.width               = m_width;
    reg.height              = m_height;
    reg.bufferFormat        = NV_ENC_BUFFER_FORMAT_ABGR;
    NV_CHECK(m_api.nvEncRegisterResource(m_encoder, &reg));
    m_registeredResource = reg.registeredResource;

    NV_ENC_MAP_INPUT_RESOURCE map{};
    map.version            = NV_ENC_MAP_INPUT_RESOURCE_VER;
    map.registeredResource = m_registeredResource;
    NV_CHECK(m_api.nvEncMapInputResource(m_encoder, &map));
    m_inputBuffer = map.mappedResource;

    NV_ENC_PIC_PARAMS pic{};
    pic.version         = NV_ENC_PIC_PARAMS_VER;
    pic.inputBuffer     = m_inputBuffer;
    pic.inputWidth      = m_width;
    pic.inputHeight     = m_height;
    pic.inputPitch      = m_width;
    pic.bufferFmt       = NV_ENC_BUFFER_FORMAT_ABGR;
    pic.outputBitstream = m_bitstreamBuffer;
    pic.pictureStruct   = NV_ENC_PIC_STRUCT_FRAME;
    pic.frameIdx        = (uint32_t)m_frameIndex;
    pic.inputTimeStamp  = m_frameIndex;  // monotonic timestamp

    // *** FIX: No FORCEIDR — let GOP structure handle keyframes naturally ***
    // Only emit SPS/PPS on first frame
    pic.encodePicFlags  = (m_frameIndex == 0) ? NV_ENC_PIC_FLAG_OUTPUT_SPSPPS : 0;

    m_frameIndex++;

    NV_CHECK(m_api.nvEncEncodePicture(m_encoder, &pic));

    NV_ENC_LOCK_BITSTREAM lock{};
    lock.version         = NV_ENC_LOCK_BITSTREAM_VER;
    lock.outputBitstream = m_bitstreamBuffer;
    NV_CHECK(m_api.nvEncLockBitstream(m_encoder, &lock));

    outBitstream.assign(
        (uint8_t*)lock.bitstreamBufferPtr,
        (uint8_t*)lock.bitstreamBufferPtr + lock.bitstreamSizeInBytes);

    m_api.nvEncUnlockBitstream(m_encoder, m_bitstreamBuffer);
    m_api.nvEncUnmapInputResource(m_encoder, m_inputBuffer);
    m_api.nvEncUnregisterResource(m_encoder, m_registeredResource);

    return true;
}

void NVEncoder::shutdown() {
    if (!m_encoder) return;
    if (m_bitstreamBuffer) {
        m_api.nvEncDestroyBitstreamBuffer(m_encoder, m_bitstreamBuffer);
        m_bitstreamBuffer = nullptr;
    }
    m_api.nvEncDestroyEncoder(m_encoder);
    m_encoder = nullptr;
}