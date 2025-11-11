#pragma once
#include "../render/DxUtil.h"
#include "MeshData.h"
#include <DirectXMath.h>


struct MeshGPU {
	ComPtr<ID3D11Buffer> vb;
	ComPtr<ID3D11Buffer> ib;
	UINT vertexCount{};
	UINT indexCount{};
};


struct Material {
	float color[3]{ 1,1,1 };
	float alpha{ 1.0f };
};


struct Transform {
	DirectX::XMFLOAT4X4 world{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
};


class Mesh {
public:
	MeshGPU gpu;
	Material mat;
	Transform xform;
};


class MeshFactory {
public:
	static Mesh CreateFrom(ID3D11Device* dev, const MeshData& data);
};