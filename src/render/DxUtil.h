#pragma once
#include <wrl/client.h>
#include <string>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <fmt/core.h>


#ifndef HR_CHECK
#define HR_CHECK(x) do { HRESULT _hr = (x); if (FAILED(_hr)) { \
throw std::runtime_error(fmt::format("HRESULT 0x{:08X} at {}:{}", (unsigned)_hr, __FILE__, __LINE__)); } } while(0)
#endif


using Microsoft::WRL::ComPtr;


namespace DxUtil {
	inline void SetDebugName(ID3D11DeviceChild* obj, const std::string& name) {
#ifdef _DEBUG
		obj->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.size(), name.c_str());
#endif
	}
}