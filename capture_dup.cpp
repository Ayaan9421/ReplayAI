#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.metadata.h>
#include <winrt/windows.graphics.capture.h>
#include <winrt/windows.graphics.directx.direct3d11.h>
#include <wrl.h>

#include <chrono>
#include <codecvt>
#include <fstream>
#include <iostream>
#include <locale>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowsapp")

using namespace std;
using Microsoft::WRL::ComPtr;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

static void PrintHR(const char* msg, HRESULT hr) { cout << msg << " hr=0x" << hex << hr << dec << endl; }

static bool SaveBGRA8AsBMP(const char* filename, UINT width, UINT height, const BYTE* pixels, UINT rowPitch) {
    BITMAPFILEHEADER bfh{};
    BITMAPINFOHEADER bih{};
    DWORD pixelDataSize = rowPitch * height;
    bfh.bfType = 0x4D42;  // 'BM'
    bfh.bfOffBits = sizeof(bfh) + sizeof(bih) + 4 * sizeof(DWORD);
    bfh.bfSize = bfh.bfOffBits + pixelDataSize;

    bih.biSize = sizeof(bih);
    bih.biWidth = static_cast<LONG>(width);
    bih.biHeight = static_cast<LONG>(height);
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_BITFIELDS;
    bih.biSizeImage = pixelDataSize;

    DWORD rMask = 0x00FF0000, gMask = 0x0000FF00, bMask = 0x000000FF, aMask = 0xFF000000;

    ofstream ofs(filename, ios::binary);
    if (!ofs) return false;

    ofs.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
    ofs.write(reinterpret_cast<const char*>(&bih), sizeof(bih));
    ofs.write(reinterpret_cast<const char*>(&rMask), sizeof(rMask));
    ofs.write(reinterpret_cast<const char*>(&gMask), sizeof(gMask));
    ofs.write(reinterpret_cast<const char*>(&bMask), sizeof(bMask));
    ofs.write(reinterpret_cast<const char*>(&aMask), sizeof(aMask));

    for (int y = static_cast<int>(height) - 1; y >= 0; --y) {
        const char* row = reinterpret_cast<const char*>(pixels + static_cast<size_t>(y) * rowPitch);
        ofs.write(row, rowPitch);
    }

    ofs.close();
    return true;
}

int wmain() {
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    HRESULT hr = S_OK;

    ComPtr<IDXGIFactory1> factory;
    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        PrintHR("CreateDXGIFactory1 failed", hr);
        return -1;
    }

    const UINT VENDOR_NVIDIA = 0x10DE;  // 4318
    const UINT VENDOR_AMD = 0x1002;     // 4098
    const UINT VENDOR_INTEL = 0x8086;   // 32902
    const unsigned long long DEDICATED_THRESHOLD_BYTES = 32ull * 1024ull * 1024ull;

    IDXGIAdapter1* adpt = nullptr;
    UINT i = 0;
    vector<DXGI_ADAPTER_DESC1> descs;
    vector<IDXGIAdapter1*> adapterList;

    while (factory->EnumAdapters1(i, &adpt) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adpt->GetDesc1(&desc);

        descs.push_back(desc);
        adapterList.push_back(adpt);
        i++;
        adpt = nullptr;
    }

    if (adapterList.empty()) {
        cout << "No GPU Adapters found." << endl;
        factory->Release();
        return -1;
    }

    int chosenAdapterIndex = -1;
    for (size_t pass = 0; pass < 3 && chosenAdapterIndex == -1; ++pass) {
        for (size_t i = 0; i < adapterList.size(); ++i) {
            const DXGI_ADAPTER_DESC1& d = descs[i];
            if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if (pass == 0) {
                if (d.VendorId == VENDOR_NVIDIA) {
                    chosenAdapterIndex = (int)i;
                    break;
                }
            } else if (pass == 1) {
                if (d.VendorId == VENDOR_AMD) {
                    chosenAdapterIndex = (int)i;
                    break;
                }
            } else {
                if ((unsigned long long)d.DedicatedVideoMemory > DEDICATED_THRESHOLD_BYTES) {
                    chosenAdapterIndex = (int)i;
                    break;
                }
            }
        }
    }

    if (chosenAdapterIndex == -1) {
        for (size_t i = 0; i < adapterList.size(); ++i) {
            if (!(descs[i].Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                chosenAdapterIndex = (int)i;
                break;
            }
        }
        if (chosenAdapterIndex == -1) chosenAdapterIndex = 0;
    }
    cout << "Selected adapter index: " << chosenAdapterIndex << "\n";

    ComPtr<IDXGIAdapter1> adapter;
    hr = factory->EnumAdapters1(chosenAdapterIndex, adapter.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("EnumAdapters failed", hr);
        return -1;
    }

    ComPtr<ID3D11Device> d3dDevice;
    ComPtr<ID3D11DeviceContext> d3dContext;
    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

    hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, createFlags, featureLevels,
                           ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, d3dDevice.GetAddressOf(), nullptr,
                           d3dContext.GetAddressOf());

    if (FAILED(hr)) {
        PrintHR("D3D11CreateDevice failed", hr);
        return -1;
    }
    cout << "D3D11 device created.\n";

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3dDevice.As(&dxgiDevice);  // calls QueryInterface under the hood
    if (FAILED(hr)) {
        PrintHR("As(IDXGIDevice) failed", hr);
        return hr;
    }

    IInspectable* inspectableDevice = nullptr;
    HRESULT hrCreateDevice = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), &inspectableDevice);
    if (FAILED(hrCreateDevice) || !inspectableDevice) {
        PrintHR("CreateDirect3D11DeviceFromDXGIDevice failed", hrCreateDevice);
        return -1;
    }

    // Wrap in winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice
    winrt::com_ptr<::IInspectable> inspectable_cp;
    inspectable_cp.copy_from(inspectableDevice);
    IDirect3DDevice d3dDeviceWinRT = inspectable_cp.as<IDirect3DDevice>();
    if (!d3dDeviceWinRT) {
        cout << "d3dDeviceWinRT is null!\n";
    } else {
        cout << "d3dDeviceWinRT OK\n";
    }
    // Get primary monitor HMONITOR
    HMONITOR hm = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    if (!hm) {
        cout << "MonitorFromWindow returned null HMONITOR\n";
        return -1;
    }

    // Activation factory for GraphicsCaptureItem and get IGraphicsCaptureItemInterop
    winrt::com_ptr<IGraphicsCaptureItemInterop> itemInterop;
    hr = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>().get()->QueryInterface(
        IID_PPV_ARGS(itemInterop.put()));

    if (FAILED(hr)) {
        PrintHR("get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop> failed", hr);
        return -1;
    }

    // Create the GraphicsCaptureItem for the monitor
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{nullptr};
    hr = itemInterop->CreateForMonitor(hm, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
                                       reinterpret_cast<void**>(winrt::put_abi(item)));
    if (FAILED(hr) || !item) {
        PrintHR("IGraphicsCaptureItemInterop::CreateForMonitor failed", hr);
        return -1;
    }
    cout << "Created GraphicsCaptureItem for primary monitor.\n";

    // --- Create a frame pool and session ---
    // Pixel format: B8G8R8A8UIntNormalized is common (BGRA8)
    auto pixelFormat = winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized;
    const UINT32 frameCount = 2;  // number of buffers in the pool

    auto itemSize = item.Size();
    cout << "Item size: " << itemSize.Width << "x" << itemSize.Height << "\n";
    if (itemSize.Width == 0 || itemSize.Height == 0) {
        cout << "GraphicsCaptureItem returned zero size; cannot create frame pool\n";
        return -1;
    }
    Direct3D11CaptureFramePool framePool = Direct3D11CaptureFramePool::Create(
        d3dDeviceWinRT, pixelFormat, frameCount, {static_cast<int>(itemSize.Width), static_cast<int>(itemSize.Height)});
    if (!framePool) {
        cout << "Failed to create frame pool\n";
        return -1;
    }
    cout << "pixelFormat: " << static_cast<int>(pixelFormat) << "\n";
    cout << "frameCount: " << frameCount << "\n";

    GraphicsCaptureSession session = framePool.CreateCaptureSession(item);
    if (!session) {
        cout << "session is null\n";
        return -1;
    }
    if (!framePool) {
        cout << "framePool is null\n";
        return -1;
    }
    bool frameArrived = false;
    winrt::Windows::Foundation::IAsyncAction waitAction = nullptr;

    // Event handler for FrameArrived
    auto token = framePool.FrameArrived(
        winrt::auto_revoke, [&](winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
                                winrt::Windows::Foundation::IInspectable const& args) {
            try {
                cout << "[FrameArrived] handler invoked\n";

                auto frame = sender.TryGetNextFrame();
                if (!frame) {
                    cout << "[FrameArrived] TryGetNextFrame returned null\n";
                    return;
                }
                auto surface = frame.Surface();  // IDirect3DSurface
                if (!surface) {
                    cout << "[FrameArrived] frame.Surface() returned null\n";
                    return;
                }
                cout << "[FrameArrived] Got frame and surface\n";

                // Get IDXGISurface from the IDirect3DSurface using the interop helper
                winrt::Windows::Foundation::IInspectable surfaceInspectable = surface;
                ::IInspectable* rawSurfaceInspect =
                    reinterpret_cast<::IInspectable*>(winrt::get_abi(surfaceInspectable));
                // Use CreateDirect3D11SurfaceFromDXGISurface to convert if needed, but easier route:
                // Use IDirect3DDxgiInterfaceAccess to get the IDXGISurface from the WinRT surface object.
                // Query for IDirect3DDxgiInterfaceAccess
                winrt::com_ptr<::IUnknown> unk = surface.as<::IInspectable>();
                // IDirect3DDxgiInterfaceAccess GUID:
                GUID iid_Direct3DDxgiInterfaceAccess;
                // ABI interface IID for IDirect3DDxgiInterfaceAccess is A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1
                iid_Direct3DDxgiInterfaceAccess = {
                    0xA9B3D012, 0x3DF2, 0x4EE3, {0xB8, 0xD1, 0x86, 0x95, 0xF4, 0x57, 0xD3, 0xC1}};
                ::IUnknown* rawUnk = nullptr;

                HRESULT hrq = rawSurfaceInspect->QueryInterface(__uuidof(IUnknown), reinterpret_cast<void**>(&rawUnk));
                if (FAILED(hrq) || !rawUnk) {
                    PrintHR("Failed to get IUnknown from surface", hrq);
                } else {
                    // Query for IDirect3DDxgiInterfaceAccess
                    struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")) IDirect3DDxgiInterfaceAccess
                        : IUnknown {
                        virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID iid, void** object) = 0;
                    };
                    IDirect3DDxgiInterfaceAccess* access = nullptr;
                    hrq = rawUnk->QueryInterface(__uuidof(IDirect3DDxgiInterfaceAccess),
                                                 reinterpret_cast<void**>(&access));

                    if (SUCCEEDED(hrq) && access) {
                        IDXGISurface* dxgiSurface = nullptr;
                        hrq = access->GetInterface(__uuidof(IDXGISurface), reinterpret_cast<void**>(&dxgiSurface));
                        if (SUCCEEDED(hrq) && dxgiSurface) {
                            // Create a CPU-readable staging texture and copy
                            DXGI_SURFACE_DESC sdesc{};
                            hrq = dxgiSurface->GetDesc(&sdesc);  // correct descriptor type
                            if (FAILED(hrq)) { PrintHR("dxgiSurface->GetDesc failed", hrq); }  // use surface desc
                            // create D3D11 texture2D from IDXGISurface by QueryInterface to ID3D11Texture2D
                            // But simpler: create a staging texture with same size/format and CopyResource from a
                            // texture created from dxgiSurface. We can get texture via QueryInterface
                            ID3D11Texture2D* srcTex = nullptr;
                            HRESULT hrTex = dxgiSurface->QueryInterface(__uuidof(ID3D11Texture2D),
                                                                        reinterpret_cast<void**>(&srcTex));
                            if (SUCCEEDED(hrTex) && srcTex) {
                                D3D11_TEXTURE2D_DESC srcDesc;
                                srcTex->GetDesc(&srcDesc);
                                D3D11_TEXTURE2D_DESC stagingDesc = srcDesc;
                                stagingDesc.Usage = D3D11_USAGE_STAGING;
                                stagingDesc.BindFlags = 0;
                                stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                                stagingDesc.MiscFlags = 0;
                                ComPtr<ID3D11Texture2D> staging;
                                HRESULT hrCreate =
                                    d3dDevice->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf());
                                if (SUCCEEDED(hrCreate)) {
                                    // copy
                                    d3dContext->CopyResource(staging.Get(), srcTex);
                                    // map
                                    D3D11_MAPPED_SUBRESOURCE mapped{};
                                    HRESULT hrMap = d3dContext->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
                                    if (SUCCEEDED(hrMap)) {
                                        // Save BMP
                                        bool ok = SaveBGRA8AsBMP("wgc_capture.bmp", srcDesc.Width, srcDesc.Height,
                                                                 reinterpret_cast<const BYTE*>(mapped.pData),
                                                                 static_cast<UINT>(mapped.RowPitch));
                                        if (ok)
                                            cout << "Saved wgc_capture.bmp (" << srcDesc.Width << "x" << srcDesc.Height
                                                 << ")\n";
                                        else
                                            cout << "Failed saving BMP\n";
                                        d3dContext->Unmap(staging.Get(), 0);
                                        frameArrived = true;
                                        if (session) session.Close();
                                        if (framePool) framePool.Close();
                                    } else {
                                        PrintHR("Map staging failed", hrMap);
                                    }
                                } else {
                                    PrintHR("Create staging texture failed", hrCreate);
                                }
                                srcTex->Release();
                            } else {
                                PrintHR("QueryInterface to ID3D11Texture2D failed", hrTex);
                            }
                            dxgiSurface->Release();
                        } else {
                            PrintHR("GetInterface(IDXGISurface) failed", hrq);
                        }
                        access->Release();
                    } else {
                        PrintHR("QueryInterface(IDirect3DDxgiInterfaceAccess) failed", hrq);
                    }
                    rawUnk->Release();
                }

                // We handled one frame; drop it.
            } catch (const winrt::hresult_error& e) {
                cout << "Frame handling exception: " << e.message().c_str() << "\n";
            }
        });

    // Start capturing
    session.StartCapture();
    cout << "Capture started; waiting for first frame...\n";

    const int TIMEOUT_MS = 5000;
    auto start = chrono::steady_clock::now();
    MSG msg;
    while (!frameArrived) {
        // process any pending Win32 messages (if any)
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // break on timeout
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count();
        if (elapsed > TIMEOUT_MS) break;
        // sleep a little so CPU isn't burned
        this_thread::sleep_for(chrono::milliseconds(5));
    }

    if (!frameArrived) {
        cout << "Timed out waiting for frame.\n";
    } else {
        cout << "Got frame.\n";
    }

    // Stop session and cleanup
    if (session) session.Close();
    if (framePool) framePool.Close();
    cout << "Done.\n";
    return frameArrived ? 0 : -1;
}