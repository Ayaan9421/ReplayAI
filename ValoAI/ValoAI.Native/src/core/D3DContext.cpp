#include "D3DContext.h"
#include<dxgi1_2.h>
#include<iostream>
#include<vector>

using Microsoft::WRL::ComPtr;

D3DContext::D3DContext() = default;
D3DContext::~D3DContext() = default;

bool D3DContext::initialize(){
    HRESULT hr = S_OK;

    // Threshold to classify GPU as "dedicated" (32 MB)
    const unsigned long long DEDICATED_THRESHOLD_BYTES = 32ull * 1024ull * 1024ull;
    
    // Vendor IDs
    const UINT VENDOR_NVIDIA  = 0x10DE;      // 4318
    const UINT VENDOR_AMD     = 0x1002;      // 4098
    const UINT VENDOR_INTEL   = 0x8086;      // 32902

    // Create DXGI factory
    ComPtr<IDXGIFactory1> factory;
    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()));
    if(FAILED(hr)) {
        cout << "[D3DContext] Failed to create DXGI Factory" << endl;
        return false;
    }

    ComPtr<IDXGIAdapter1> adapter = nullptr;
    UINT i = 0;
    vector<DXGI_ADAPTER_DESC1> descs;
    vector<ComPtr<IDXGIAdapter1>> adapterList;

    while(factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        descs.push_back(desc);
        adapterList.push_back(adapter);
        i++;
        adapter = nullptr;
    }

    if(adapterList.empty()) {
        cout << "[D3DContext] No GPU Adapters Found." << endl;
        factory->Release();
        return false;
    }

    int chosenAdapterIndex = -1;
    for(size_t pass = 0; pass < 3 && chosenAdapterIndex == -1; pass++) {
        for(size_t i=0;i<adapterList.size(); i++) {
            const DXGI_ADAPTER_DESC1 &d = descs[i];
            if(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if(pass == 0) {
                if(d.VendorId == VENDOR_NVIDIA) {   chosenAdapterIndex = (int)i;    break;     }    
            } else if(pass == 1) {
                if(d.VendorId == VENDOR_AMD) {   chosenAdapterIndex = (int)i;    break;     }
            } else {
                if ((unsigned long long)d.DedicatedVideoMemory > DEDICATED_THRESHOLD_BYTES) {   chosenAdapterIndex = (int)i;    break;     }
            }
        }
    }
    if(chosenAdapterIndex == -1) {
        for (size_t i = 0;i < adapterList.size(); ++i) {
            if (!(descs[i].Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                chosenAdapterIndex = (int)i;
                break;
            }
        }
        if (chosenAdapterIndex == -1) chosenAdapterIndex = 0;
    }
    
    adapter = nullptr;
    hr = factory->EnumAdapters1(chosenAdapterIndex, adapter.GetAddressOf());
    if(FAILED(hr)){
        cout << "[D3DContext] EnumAdapters Failed. hr=0x" << hex << hr << dec << endl;
        return false;
    }

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    hr = D3D11CreateDevice(
        adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        flags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        m_device.GetAddressOf(),
        nullptr,
        m_context.GetAddressOf()
    );
    if(FAILED(hr)) {
        cout << "[D3DContext] Failed to create D3D11 Device." << endl;
        return false;
    }

    cout << "[D3DContext] D3D11 Device Created" << endl;
    return true;
}

ID3D11Device* D3DContext::device() const {
    return m_device.Get();
}

ID3D11DeviceContext* D3DContext::context() const {
    return m_context.Get();
}