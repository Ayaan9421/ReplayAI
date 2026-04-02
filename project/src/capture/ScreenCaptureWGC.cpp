#include "ScreenCaptureWGC.h"

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <windows.h>
#include <winrt/Windows.Foundation.h>

#include <iostream>

#include "../core/D3DContext.h"

using namespace std;
using namespace winrt;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

ScreenCaptureWGC::ScreenCaptureWGC() = default;
ScreenCaptureWGC::~ScreenCaptureWGC() = default;

bool ScreenCaptureWGC::initialize(D3DContext* d3d) {
    m_d3d = d3d;

    if (!createCaptureItem()) { return false; }

    if (!createFramePool()) { return false; }

    m_session = m_framePool.CreateCaptureSession(m_item);
    m_session.IsBorderRequired(false);

    m_session.StartCapture();

    cout << "[ScreenCaptureWGC] Capture started" << endl;
    return true;
}

bool ScreenCaptureWGC::createCaptureItem() {
    // Primary Monitor
    HMONITOR monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    if (!monitor) {
        cout << "[ScreenCaptureWGC] Failed to get primary monitor" << endl;
        return false;
    }

    auto factory = get_activation_factory<GraphicsCaptureItem>();
    auto interop = factory.as<IGraphicsCaptureItemInterop>();

    HRESULT hr = interop->CreateForMonitor(monitor, guid_of<GraphicsCaptureItem>(), put_abi(m_item));

    if (FAILED(hr) || !m_item) {
        cout << "[WGC] CreateForMonitor Failed" << endl;
        return false;
    }

    return true;
}

bool ScreenCaptureWGC::createFramePool() {
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    m_d3d->device()->QueryInterface(IID_PPV_ARGS(&dxgiDevice));

    winrt::com_ptr<IInspectable> inspectable;
    CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put());

    auto d3dDeviceWinRT = inspectable.as<IDirect3DDevice>();

    auto size = m_item.Size();
    if (size.Width == 0 || size.Height == 0) {
        cout << "[WGC] Invalid Capture Size" << endl;
        return false;
    }

    m_framePool =
        Direct3D11CaptureFramePool::Create(d3dDeviceWinRT, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);

    // FrameArrived Callback
    m_framePool.FrameArrived([this](auto& sender, auto&) {
        auto frame = sender.TryGetNextFrame();
        if (!frame) return;

        auto surface = frame.Surface();
        if (!surface) return;

        // Get raw IInspectable
        winrt::com_ptr<IInspectable> inspectable = surface.as<IInspectable>();

        struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")) IDirect3DDxgiInterfaceAccess : IUnknown {
            virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID iid, void** object) = 0;
        };

        // Query COM Interfafce
        IDirect3DDxgiInterfaceAccess* access = nullptr;
        HRESULT hr =
            inspectable->QueryInterface(__uuidof(IDirect3DDxgiInterfaceAccess), reinterpret_cast<void**>(&access));

        if (FAILED(hr) || !access) return;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        hr = access->GetInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(tex.GetAddressOf()));

        access->Release();

        if (FAILED(hr) || !tex) return;

        m_latestFrame = tex;
    });

    return true;
}

ID3D11Texture2D* ScreenCaptureWGC::getFrame() { return m_latestFrame.Get(); }
