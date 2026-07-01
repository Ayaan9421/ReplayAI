#pragma once

#include<wrl.h>
#include<d3d11.h>

using namespace std;


// --------------------------------------------------
// D3DContext
// Owns the D3D11 Device + immediate Context
// --------------------------------------------------

class D3DContext{
    public:
        D3DContext();
        ~D3DContext();

        bool initialize();

        ID3D11Device* device() const;
        ID3D11DeviceContext* context() const;

    private:
        Microsoft::WRL::ComPtr<ID3D11Device> m_device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
};