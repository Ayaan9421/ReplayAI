#include<iostream>
#include<vector>
#include<string>
#include<locale>
#include<codecvt>
#include<d3d11.h>
#include<dxgi1_2.h>
#include<wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace std; 

static string WideToUTF8(const wchar_t* wstr){
    wstring ws(wstr);
    wstring_convert<codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(ws);
}

int main(){

    HRESULT hr;

    // Threshold to classify GPU as "dedicated" (32 MB)
    const unsigned long long DEDICATED_THRESHOLD_BYTES = 32ull * 1024ull * 1024ull;

    // Vendor IDs (decimal)
    const UINT VENDOR_NVIDIA = 0x10DE; // 4318
    const UINT VENDOR_AMD    = 0x1002; // 4098
    const UINT VENDOR_INTEL  = 0x8086; // 32902

    // 1. Create dxgi factory
    IDXGIFactory1* factory = nullptr;
    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);

    if (FAILED(hr)) {
        cout << "Failed to create DXGI Factory." << endl;
        return -1;
    }

    cout << "Listing all GPU adapters:" <<endl;

    // 2. Listing all adapters
    IDXGIAdapter1* adapter = nullptr;
    UINT i = 0;
    vector<DXGI_ADAPTER_DESC1> descs;
    vector<IDXGIAdapter1*> adapterList;
    
    while(factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND){
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        wstring name_encoded(desc.Description);
        wstring_convert<codecvt_utf8<wchar_t>> converter;

        string name = converter.to_bytes(name_encoded);
        
        cout << "Adapter " << i << ": " << endl;
        cout << "Name: " << name << endl;
        cout << "Vendor ID: " << desc.VendorId << endl;
        cout << "Device ID: " << desc.DeviceId << endl;
        cout << "Dedicated : " << (unsigned long long)desc.DedicatedVideoMemory << " bytes\n";
        cout << "Flags     : " << desc.Flags << "\n\n";
        cout << "----------------------------------------" << endl;
        
        descs.push_back(desc);
        adapterList.push_back(adapter);
        i++;
        adapter = nullptr;
    }

    // 3. Create D3D11 device for the first adapter
    if (adapterList.empty()) {
        cout << "No GPU Adapters found." << endl;
        factory->Release();
        return -1;
    }

    // cout << VENDOR_NVIDIA << " " << VENDOR_AMD << " " << VENDOR_INTEL << endl;
    
    // select the adapter
    int chosenAdapterIndex = -1;
    for (size_t pass = 0; pass < 3 && chosenAdapterIndex == -1; ++pass) {
        for (size_t i = 0; i < adapterList.size(); ++i) {
            const DXGI_ADAPTER_DESC1& d = descs[i];

            // skip software adapters always
            if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            // cout << pass << ": Checking adapter i = " << i << " vendor= "<< d.VendorId << endl;
            if (pass == 0) {
                // prefer NVIDIA first
                // cout << (d.VendorId == VENDOR_NVIDIA) << endl;
                if (d.VendorId == VENDOR_NVIDIA) { 
                    chosenAdapterIndex = (int)i; 
                    // cout << "Chosen Adapter i = " << i << " chosenAdapterIndex = " << chosenAdapterIndex << endl;
                    break; 
                }
            } else if (pass == 1) {
                // then AMD
                if (d.VendorId == VENDOR_AMD) { 
                    chosenAdapterIndex = (int)i; 
                    break; 
                }
            } else {
                // pass 2: fallback to dedicated-memory heuristic
                if ((unsigned long long)d.DedicatedVideoMemory > DEDICATED_THRESHOLD_BYTES) {
                    chosenAdapterIndex = (int)i;
                    break;
                }
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
    cout << "Selected adapter index: " << chosenAdapterIndex << "\n";
    cout << "Selected adapter name : " << WideToUTF8(descs[chosenAdapterIndex].Description) << "\n\n";

    IDXGIAdapter1* mainAdapter = adapterList[chosenAdapterIndex];
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    hr = D3D11CreateDevice(
        mainAdapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &device,
        nullptr,
        &context
    );

    if(FAILED(hr)){
        cout << "Failed to create D3D11 Device." << endl;
    } else {
        cout << "D3D11 Device created successfully for the first adapter." << endl;
    }

    // 4. Cleanup
    for (auto a: adapterList) a->Release();

    if (device) device->Release();
    if (context) context->Release();

    factory->Release();

    return 0;
}