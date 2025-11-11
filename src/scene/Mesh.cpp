#include "Mesh.h"
#include <stdexcept>
using namespace DirectX;


struct VertexPN { float px, py, pz; float nx, ny, nz; };


Mesh MeshFactory::CreateFrom(ID3D11Device* dev, const MeshData& data)
{

	if (!dev)
		throw std::invalid_argument("MeshFactory::CreateFrom: device == nullptr (appelez addMesh APRES l'initialisation du renderer)");


	if (data.positions.empty() || data.normals.empty() || data.indices.empty())
		throw std::runtime_error("MeshData incomplet");
	if (data.positions.size() != data.normals.size())
		throw std::runtime_error("positions != normals");


	std::vector<VertexPN> vertices;
	vertices.reserve(data.positions.size());
	for (size_t i = 0; i < data.positions.size(); ++i) {
		VertexPN v{};
		v.px = data.positions[i].x; v.py = data.positions[i].y; v.pz = data.positions[i].z;
		v.nx = data.normals[i].x; v.ny = data.normals[i].y; v.nz = data.normals[i].z;
		vertices.push_back(v);
	}


	Mesh mesh{};


	D3D11_BUFFER_DESC vbd{}; vbd.Usage = D3D11_USAGE_DEFAULT; vbd.ByteWidth = UINT(vertices.size() * sizeof(VertexPN));
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vinit{}; vinit.pSysMem = vertices.data();
	HR_CHECK(dev->CreateBuffer(&vbd, &vinit, &mesh.gpu.vb));
	DxUtil::SetDebugName(mesh.gpu.vb.Get(), "VB");


	D3D11_BUFFER_DESC ibd{}; ibd.Usage = D3D11_USAGE_DEFAULT; ibd.ByteWidth = UINT(data.indices.size() * sizeof(uint32_t));
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA iinit{}; iinit.pSysMem = data.indices.data();
	HR_CHECK(dev->CreateBuffer(&ibd, &iinit, &mesh.gpu.ib));
	DxUtil::SetDebugName(mesh.gpu.ib.Get(), "IB");


	mesh.gpu.vertexCount = (UINT)vertices.size();
	mesh.gpu.indexCount = (UINT)data.indices.size();


	return mesh;
}