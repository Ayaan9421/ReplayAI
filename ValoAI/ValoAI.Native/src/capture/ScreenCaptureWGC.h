#pragma once
#include <wrl.h>
#include <d3d11.h>
#include <atomic>
#include <winrt/windows.graphics.capture.h>
#include <winrt/windows.graphics.directx.direct3d11.h>

class D3DContext;

class ScreenCaptureWGC {
public:
    ScreenCaptureWGC();
    ~ScreenCaptureWGC();

    bool initialize(D3DContext* d3d);
    ID3D11Texture2D* getFrame();
    bool hasNewFrame();  // returns true once per new WGC frame

private:
    bool createCaptureItem();
    bool createFramePool();

private:
    D3DContext* m_d3d;
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem m_item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool m_framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession m_session{ nullptr };

    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_latestFrame;
    std::atomic<bool> m_newFrameAvailable{ false };
};