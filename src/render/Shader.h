#pragma once
#include "DxUtil.h"
#include <string>

class Shader {
public:
    struct VS {
        ComPtr<ID3D11VertexShader> shader;
        ComPtr<ID3D11InputLayout>  layout;
    };
    struct PS {
        ComPtr<ID3D11PixelShader>  shader;
    };

    // Charge un Vertex Shader + InputLayout
    // - path : ex. L"shaders/mesh.hlsl"
    // - layout/count : doit matcher les semantics du VS (POSITION, NORMAL, etc.)
    // - entry : point d'entree HLSL (par d�faut "VSMain")
    static VS LoadVS(ID3D11Device* dev, const std::wstring& path,
        const D3D11_INPUT_ELEMENT_DESC* layout, UINT count,
        const char* entry = "VSMain");

    // Charge un Pixel Shader
    static PS LoadPS(ID3D11Device* dev, const std::wstring& path,
        const char* entry = "PSMain");
};
