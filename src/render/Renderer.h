#pragma once
#include "DxUtil.h"
#include "Shader.h"
#include "../scene/Scene.h"
#include "../scene/Camera.h"
#include <DirectXMath.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <dxgi.h>

// === Constant buffers (row_major côté HLSL) ===
struct alignas(16) CBFrame { DirectX::XMFLOAT4X4 View; DirectX::XMFLOAT4X4 Proj; float LightDir[3]; float _pad0 = 0; };
struct alignas(16) CBObject { DirectX::XMFLOAT4X4 World; };
struct alignas(16) CBMaterial { float BaseColor[3]; float _pad1 = 0; };
// alignas(16) + tailles multiple de 16 safe pour D3D11.


class Renderer {
public:
	void initialize(HWND hwnd, int width, int height);
	void resize(int width, int height);

	void toggleProjection();
	void render(const Scene& scene);
	OrbitCamera& camera() { return cam; }
	ID3D11Device* device() const { return dev.Get(); }


private:
	void createSwapchainAndTargets(HWND hwnd, int w, int h);

	bool d2dReady = false; // vrai si D2D/DWrite sont init OK

	// check 
	HWND hwnd_ = nullptr;
	int  bbWidth_ = 0, bbHeight_ = 0;

	void safeReleaseBackbufferBindings();
	void handleDeviceLost(); // recrée device/swapchain/ressources
	// end check

	ComPtr<ID3D11Device> dev;
	ComPtr<ID3D11DeviceContext> ctx;
	ComPtr<IDXGISwapChain> swap;
	ComPtr<ID3D11RenderTargetView> rtv;
	ComPtr<ID3D11DepthStencilView> dsv;

	//culling
	ComPtr<ID3D11RasterizerState> rsState;


	Shader::VS vs; Shader::PS ps;


	struct CameraCB { DirectX::XMFLOAT4X4 view; DirectX::XMFLOAT4X4 proj; float eye[3]; float _pad; };
	struct ObjectCB { DirectX::XMFLOAT4X4 world; float color[3]; float alpha; };


	ComPtr<ID3D11Buffer> camCB;
	ComPtr<ID3D11Buffer> objCB;
	ComPtr<ID3D11Buffer> matCB;

	// depth-stencil state
	ComPtr<ID3D11DepthStencilState> dss;


	D3D11_VIEWPORT vp{};
	OrbitCamera cam;

	/*****************Overlay*****************/
	Microsoft::WRL::ComPtr<ID2D1Factory1>       d2dFactory;
	Microsoft::WRL::ComPtr<ID2D1Device>         d2dDevice;
	Microsoft::WRL::ComPtr<ID2D1DeviceContext>  d2dCtx;
	Microsoft::WRL::ComPtr<IDWriteFactory>      dwFactory;
	Microsoft::WRL::ComPtr<IDWriteTextFormat>   dwTextFmt;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>dwBrush;
	Microsoft::WRL::ComPtr<ID2D1Bitmap1>        d2dTarget;   // backbuffer as D2D bitmap

	void createD2DResources();       // init une fois
	void bindD2DTargetFromSwap();    // (re)crée le bitmap cible à chaque resize
	void drawOverlay();              // draw “Aide” avant Present
};