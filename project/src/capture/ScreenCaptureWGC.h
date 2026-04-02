#pragma once

#include <wrl.h>
#include <d3d11.h>

#include <winrt/windows.graphics.capture.h>
#include <winrt/windows.graphics.directx.direct3d11.h>

class D3DContext;

// ------------------------------------------------------------
// ScreenCaptureWGC
// Uses Windows Graphics Capture to grab desktop frames
// ------------------------------------------------------------

class ScreenCaptureWGC
{
public:
        ScreenCaptureWGC();
        ~ScreenCaptureWGC();

        // Initialize Capture for primary monitor
        bool initialize(D3DContext *d3d);

        // Returns latest BGRA Frame
        ID3D11Texture2D *getFrame();

private:
        bool createCaptureItem();
        bool createFramePool();

private:
        D3DContext *m_d3d;
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{nullptr};
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{nullptr};
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{nullptr};

        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_latestFrame;
};