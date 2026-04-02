#pragma once

#include <d3d11.h>

#include <cstdint>
#include <vector>

class IVideoEncoder {
   public:
    virtual ~IVideoEncoder() = default;

    virtual bool initialize(ID3D11Device* device, uint32_t width, uint32_t height) = 0;

    virtual bool encodeFrame(ID3D11Texture2D* inputTexture, std::vector<uint8_t>& outBitstream) = 0;

    virtual void shutdown() = 0;
};