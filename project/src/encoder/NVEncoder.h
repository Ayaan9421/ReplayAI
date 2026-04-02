#pragma once
#include <nvEncodeAPI.h>

#include <vector>

#include "IVideoEncoder.h"

class NVEncoder : public IVideoEncoder {
   public:
    NVEncoder();
    ~NVEncoder() override;

    bool initialize(ID3D11Device* device, uint32_t width, uint32_t height) override;

    bool encodeFrame(ID3D11Texture2D* inputTexture, std::vector<uint8_t>& outBitstream) override;

    void shutdown() override;

   private:
    bool initNVENC(ID3D11Device* device);
    bool createEncoder(uint32_t width, uint32_t height);

   private:
    NV_ENCODE_API_FUNCTION_LIST m_api{};
    void* m_encoder = nullptr;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    uint64_t m_frameIndex = 0;
    double m_FPS = 60.0;

    NV_ENC_INPUT_PTR m_inputBuffer = nullptr;
    NV_ENC_REGISTERED_PTR m_registeredResource = nullptr;
    NV_ENC_OUTPUT_PTR m_bitstreamBuffer = nullptr;

    NV_ENC_CONFIG m_encConfig{};
};