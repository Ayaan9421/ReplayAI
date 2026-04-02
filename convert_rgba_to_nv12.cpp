// convert_rgba_to_nv12_robust.cpp
// Build: cl /EHsc /std:c++17 convert_rgba_to_nv12_robust.cpp /link d3d11.lib dxgi.lib d3dcompiler.lib
// Robust RGBA -> NV12 compute shader demo: writes into RGBA UNORM UAVs to avoid typed-UAV issues.

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl.h>

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace std;

static void PrintHR(const char* msg, HRESULT hr) { cout << msg << " hr=0x" << hex << hr << dec << "\n"; }

// HLSL compute shader: read float4 from UNORM SRV, produce Y (in outY.r) and UV (outUV.rg) as floats 0..1
static const char* cs_hlsl = R"(
cbuffer Params : register(b0) {
    uint Width;
    uint Height;
};

Texture2D<float4> srcTex : register(t0); // read UNORM RGBA as float4 [0..1]
RWTexture2D<float4> outY : register(u0);  // we'll store Y in .r (0..1)
RWTexture2D<float4> outUV : register(u1); // store U->.r, V->.g at half res (0..1)

[numthreads(16,16,1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint sx = DTid.x;
    uint sy = DTid.y;
    if (sx >= Width || sy >= Height) return;

    float4 pix = srcTex.Load(int3(sx, sy, 0));
    float R = pix.r;
    float G = pix.g;
    float B = pix.b;

    // BT.601-ish
    float y_f =  0.257f * R + 0.504f * G + 0.098f * B + 0.0625f;
    float u_f = -0.148f * R - 0.291f * G + 0.439f * B + 0.5f;
    float v_f =  0.439f * R - 0.368f * G - 0.071f * B + 0.5f;

    outY[int2(sx, sy)] = float4(y_f, 0.0f, 0.0f, 1.0f);

    if ((sx & 1u) == 0u && (sy & 1u) == 0u) {
        float sumU = 0.0f, sumV = 0.0f;
        uint count = 0;
        for (uint yy = 0; yy < 2; ++yy) {
            for (uint xx = 0; xx < 2; ++xx) {
                uint nx = sx + xx;
                uint ny = sy + yy;
                if (nx < Width && ny < Height) {
                    float4 p = srcTex.Load(int3(nx, ny, 0));
                    float r = p.r, g = p.g, b = p.b;
                    float uf = -0.148f * r - 0.291f * g + 0.439f * b + 0.5f;
                    float vf =  0.439f * r - 0.368f * g - 0.071f * b + 0.5f;
                    sumU += uf; sumV += vf; ++count;
                }
            }
        }
        float avgU = sumU / (float)count;
        float avgV = sumV / (float)count;
        outUV[int2(sx/2, sy/2)] = float4(avgU, avgV, 0.0f, 1.0f);
    }
}
)";

int main() {
    HRESULT hr;

    // Create DXGI factory and pick adapter 0
    ComPtr<IDXGIFactory1> factory;
    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        PrintHR("CreateDXGIFactory1", hr);
        return -1;
    }
    ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(factory->EnumAdapters1(0, adapter.GetAddressOf()))) {
        cout << "No adapter 0\n";
        return -1;
    }

    // Create D3D11 device
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL fls[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
    hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags, fls, ARRAYSIZE(fls),
                           D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, ctx.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("D3D11CreateDevice", hr);
        return -1;
    }
    cout << "D3D11 device created\n";

    // Small test size (even dimensions)
    const UINT W = 8, H = 8;

    // Create source RGBA UNORM texture and fill with known pattern
    D3D11_TEXTURE2D_DESC srcDesc{};
    srcDesc.Width = W;
    srcDesc.Height = H;
    srcDesc.MipLevels = 1;
    srcDesc.ArraySize = 1;
    srcDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srcDesc.SampleDesc.Count = 1;
    srcDesc.Usage = D3D11_USAGE_DEFAULT;
    srcDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    srcDesc.CPUAccessFlags = 0;

    ComPtr<ID3D11Texture2D> srcTex;
    hr = device->CreateTexture2D(&srcDesc, nullptr, srcTex.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("CreateTexture2D src", hr);
        return -1;
    }

    vector<UINT32> pixels(W * H);
    for (UINT y = 0; y < H; ++y) {
        for (UINT x = 0; x < W; ++x) {
            UINT8 r = (x < W / 2) ? 255 : 0;
            UINT8 g = (y < H / 2) ? 255 : 0;
            UINT8 b = ((x + y) & 1) ? 255 : 0;
            UINT8 a = 255;
            pixels[y * W + x] = (a << 24) | (b << 16) | (g << 8) | (r);
        }
    }
    D3D11_BOX box{0, 0, 0, W, H, 1};
    ctx->UpdateSubresource(srcTex.Get(), 0, &box, pixels.data(), W * 4, 0);

    // SRV
    ComPtr<ID3D11ShaderResourceView> srcSRV;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = srcDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(srcTex.Get(), &srvDesc, srcSRV.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("CreateSRV", hr);
        return -1;
    }

    // Output textures: use RGBA UNORM UNIFORMLY to avoid UAV format quirks
    D3D11_TEXTURE2D_DESC yDesc{};
    yDesc.Width = W;
    yDesc.Height = H;
    yDesc.MipLevels = 1;
    yDesc.ArraySize = 1;
    yDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // store Y in .r
    yDesc.SampleDesc.Count = 1;
    yDesc.Usage = D3D11_USAGE_DEFAULT;
    yDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> yTex;
    hr = device->CreateTexture2D(&yDesc, nullptr, yTex.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("CreateTexture2D y", hr);
        return -1;
    }

    D3D11_TEXTURE2D_DESC uvDesc{};
    uvDesc.Width = W / 2;
    uvDesc.Height = H / 2;
    uvDesc.MipLevels = 1;
    uvDesc.ArraySize = 1;
    uvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // store U in .r, V in .g
    uvDesc.SampleDesc.Count = 1;
    uvDesc.Usage = D3D11_USAGE_DEFAULT;
    uvDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> uvTex;
    hr = device->CreateTexture2D(&uvDesc, nullptr, uvTex.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("CreateTexture2D uv", hr);
        return -1;
    }

    // UAVs (format is same as texture)
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavYDesc{};
    uavYDesc.Format = yDesc.Format;
    uavYDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavYDesc.Texture2D.MipSlice = 0;
    ComPtr<ID3D11UnorderedAccessView> uavY;
    hr = device->CreateUnorderedAccessView(yTex.Get(), &uavYDesc, uavY.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("CreateUAV Y", hr);
        return -1;
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavUVDesc{};
    uavUVDesc.Format = uvDesc.Format;
    uavUVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavUVDesc.Texture2D.MipSlice = 0;
    ComPtr<ID3D11UnorderedAccessView> uavUV;
    hr = device->CreateUnorderedAccessView(uvTex.Get(), &uavUVDesc, uavUV.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("CreateUAV UV", hr);
        return -1;
    }

    // Constant buffer
    struct Params {
        UINT Width;
        UINT Height;
        UINT pad[2];
    } params{W, H, 0, 0};
    D3D11_BUFFER_DESC cbd{};
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.ByteWidth = sizeof(params);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA cbdInit{&params, 0, 0};
    ComPtr<ID3D11Buffer> cb;
    hr = device->CreateBuffer(&cbd, &cbdInit, cb.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("CreateBuffer CB", hr);
        return -1;
    }

    // Compile compute shader
    ComPtr<ID3DBlob> csBlob, errBlob;
    hr = D3DCompile(cs_hlsl, strlen(cs_hlsl), nullptr, nullptr, nullptr, "main", "cs_5_0", 0, 0, csBlob.GetAddressOf(),
                    errBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errBlob) cout << "Shader error:\n" << static_cast<const char*>(errBlob->GetBufferPointer()) << "\n";
        PrintHR("D3DCompile", hr);
        return -1;
    }
    ComPtr<ID3D11ComputeShader> cs;
    hr = device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, cs.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("CreateComputeShader", hr);
        return -1;
    }

    // Bind and dispatch
    ctx->CSSetShader(cs.Get(), nullptr, 0);
    ID3D11ShaderResourceView* srvs[1] = {srcSRV.Get()};
    ctx->CSSetShaderResources(0, 1, srvs);
    ID3D11UnorderedAccessView* uavs[2] = {uavY.Get(), uavUV.Get()};
    ctx->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
    ctx->CSSetConstantBuffers(0, 1, cb.GetAddressOf());

    UINT tgX = (W + 15) / 16;
    UINT tgY = (H + 15) / 16;
    ctx->Dispatch(tgX, tgY, 1);

    // Unbind
    ID3D11UnorderedAccessView* nullUAVs[2] = {nullptr, nullptr};
    ctx->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
    ID3D11ShaderResourceView* nullSRV[1] = {nullptr};
    ctx->CSSetShaderResources(0, 1, nullSRV);
    ctx->CSSetConstantBuffers(0, 0, nullptr);
    ctx->CSSetShader(nullptr, nullptr, 0);

    // Create staging copies for readback
    D3D11_TEXTURE2D_DESC yStDesc = yDesc;
    yStDesc.Usage = D3D11_USAGE_STAGING;
    yStDesc.BindFlags = 0;
    yStDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> yStaging;
    hr = device->CreateTexture2D(&yStDesc, nullptr, yStaging.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("Create yStaging", hr);
        return -1;
    }
    D3D11_TEXTURE2D_DESC uvStDesc = uvDesc;
    uvStDesc.Usage = D3D11_USAGE_STAGING;
    uvStDesc.BindFlags = 0;
    uvStDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> uvStaging;
    hr = device->CreateTexture2D(&uvStDesc, nullptr, uvStaging.GetAddressOf());
    if (FAILED(hr)) {
        PrintHR("Create uvStaging", hr);
        return -1;
    }

    ctx->CopyResource(yStaging.Get(), yTex.Get());
    ctx->CopyResource(uvStaging.Get(), uvTex.Get());

    // Map Y (RGBA UNORM) and extract .r bytes as Y
    D3D11_MAPPED_SUBRESOURCE my{};
    hr = ctx->Map(yStaging.Get(), 0, D3D11_MAP_READ, 0, &my);
    if (FAILED(hr)) {
        PrintHR("Map yStaging", hr);
        return -1;
    }
    vector<uint8_t> yPlane(W * H);
    for (UINT row = 0; row < H; ++row) {
        uint8_t* src = reinterpret_cast<uint8_t*>(my.pData) + row * my.RowPitch;
        for (UINT col = 0; col < W; ++col) {
            // RGBA bytes: r at offset 0
            yPlane[row * W + col] = src[col * 4 + 0];
        }
    }
    ctx->Unmap(yStaging.Get(), 0);

    // Map UV (RGBA UNORM half-res), extract r->U and g->V
    D3D11_MAPPED_SUBRESOURCE muv{};
    hr = ctx->Map(uvStaging.Get(), 0, D3D11_MAP_READ, 0, &muv);
    if (FAILED(hr)) {
        PrintHR("Map uvStaging", hr);
        return -1;
    }
    vector<uint8_t> uvPlane((W / 2) * (H / 2) * 2);
    for (UINT row = 0; row < H / 2; ++row) {
        uint8_t* src = reinterpret_cast<uint8_t*>(muv.pData) + row * muv.RowPitch;
        for (UINT col = 0; col < W / 2; ++col) {
            uint8_t u = src[col * 4 + 0];
            uint8_t v = src[col * 4 + 1];
            uvPlane[(row * (W / 2) + col) * 2 + 0] = u;
            uvPlane[(row * (W / 2) + col) * 2 + 1] = v;
        }
    }
    ctx->Unmap(uvStaging.Get(), 0);

    // Print sample bytes
    cout << "Sample Y values: ";
    for (int i = 0; i < min<int>(8, (int)yPlane.size()); ++i) printf("%02X ", yPlane[i]);
    printf("\nSample UV bytes: ");
    for (int i = 0; i < min<int>(8, (int)uvPlane.size()); ++i) printf("%02X ", uvPlane[i]);
    printf("\n");

    // Build NV12 buffer
    vector<uint8_t> nv12;
    nv12.insert(nv12.end(), yPlane.begin(), yPlane.end());
    nv12.insert(nv12.end(), uvPlane.begin(), uvPlane.end());
    cout << "NV12 size = " << nv12.size() << "\n";

    cout << "Done.\n";
    return 0;
}
