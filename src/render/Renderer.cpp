#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Renderer.h"
#include <stdexcept>
#include <algorithm>
#include <cwchar>          // wcslen

// Interop DXGI/D2D/DWrite
#include <dxgi1_2.h>       // IDXGIFactory2, DXGI_SWAP_CHAIN_DESC1, CreateSwapChainForHwnd
#include <d2d1_1.h>
#include <dwrite.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

void Renderer::initialize(HWND hwnd, int width, int height)
{
    hwnd_ = hwnd;
    bbWidth_ = width;
    bbHeight_ = height;


    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    flags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT; // interop Direct2D

    D3D_FEATURE_LEVEL flv[] = { D3D_FEATURE_LEVEL_11_0 };
    HR_CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, flv, 1, D3D11_SDK_VERSION, &dev, nullptr, &ctx));

    createSwapchainAndTargets(hwnd, width, height);

    // Overlay 2D
    createD2DResources();
    bindD2DTargetFromSwap();

    // Shaders + input layout
    D3D11_INPUT_ELEMENT_DESC il[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                           D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    vs = Shader::LoadVS(dev.Get(), L"shaders/mesh.hlsl", il, _countof(il));
    ps = Shader::LoadPS(dev.Get(), L"shaders/mesh.hlsl");

    // Depth-stencil state
    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    HR_CHECK(dev->CreateDepthStencilState(&dsd, &dss));
    ctx->OMSetDepthStencilState(dss.Get(), 0);

    // Constant buffers (alignés sur 16 octets)
    auto MakeCB = [&](UINT bytes, ComPtr<ID3D11Buffer>& out) {
        D3D11_BUFFER_DESC d{};
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        d.ByteWidth = (bytes + 15) & ~15u;
        HR_CHECK(dev->CreateBuffer(&d, nullptr, &out));
    };
    MakeCB(sizeof(CBFrame),    camCB);
    MakeCB(sizeof(CBObject),   objCB);
    MakeCB(sizeof(CBMaterial), matCB);

    // Rasterizer
    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_BACK;
    rs.FrontCounterClockwise = TRUE; // CCW front
    HR_CHECK(dev->CreateRasterizerState(&rs, &rsState));
    ctx->RSSetState(rsState.Get());

    // Projection ORTHO par défaut
    const float aspect = (height > 0) ? (float(width) / float(height)) : 1.0f;
    cam.setOrthographic(/*orthoHeight*/ 4.0f, aspect, 0.1f, 1000.0f);
}

void Renderer::createSwapchainAndTargets(HWND hwnd, int w, int h)
{
    if (swap) {
        ctx->OMSetRenderTargets(0, nullptr, nullptr);
        rtv.Reset(); dsv.Reset();
        d2dTarget.Reset();
        swap.Reset();
    }

    // DXGI factory
    ComPtr<IDXGIDevice>   dxgiDev; HR_CHECK(dev.As(&dxgiDev));
    ComPtr<IDXGIAdapter>  adp;     HR_CHECK(dxgiDev->GetAdapter(&adp));
    ComPtr<IDXGIFactory1> fac1;    HR_CHECK(adp->GetParent(__uuidof(IDXGIFactory1), &fac1));
    fac1->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    // Flip-model de préférence
    ComPtr<IDXGIFactory2> fac2;
    if (SUCCEEDED(fac1.As(&fac2))) {
        DXGI_SWAP_CHAIN_DESC1 d{};
        d.Width = (UINT)w;
        d.Height = (UINT)h;
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // BGRA pour D2D
        d.SampleDesc = { 1,0 };
        d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        d.BufferCount = 2;
        d.Scaling = DXGI_SCALING_STRETCH;
        d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        ComPtr<IDXGISwapChain1> swap1;
        HR_CHECK(fac2->CreateSwapChainForHwnd(dev.Get(), hwnd, &d, nullptr, nullptr, &swap1));
        HR_CHECK(swap1.As(&swap)); // 'swap' est ComPtr<IDXGISwapChain>
    } else {
        // Fallback legacy (rarement nécessaire)
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferDesc = { (UINT)w, (UINT)h, {0,0}, DXGI_FORMAT_B8G8R8A8_UNORM };
        sd.SampleDesc = {1, 0};
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.OutputWindow = hwnd;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        HR_CHECK(fac1->CreateSwapChain(dev.Get(), &sd, &swap));
    }

    // RTV backbuffer
    ComPtr<ID3D11Texture2D> bb;
    HR_CHECK(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), &bb));
    HR_CHECK(dev->CreateRenderTargetView(bb.Get(), nullptr, &rtv));

    // DSV
    D3D11_TEXTURE2D_DESC dsd{};
    dsd.Width = w; dsd.Height = h; dsd.MipLevels = 1; dsd.ArraySize = 1;
    dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsd.SampleDesc = {1, 0};
    dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> ds;
    HR_CHECK(dev->CreateTexture2D(&dsd, nullptr, &ds));
    HR_CHECK(dev->CreateDepthStencilView(ds.Get(), nullptr, &dsv));

    // Viewport
    vp = { 0,0,(FLOAT)w,(FLOAT)h, 0.0f, 1.0f };

    // Cible D2D
    bindD2DTargetFromSwap();
}

void Renderer::createD2DResources()
{
    d2dReady = false; // on (re)part de zéro

    ComPtr<IDXGIDevice> dxgiDev;
    HRESULT hr = dev.As(&dxgiDev);
    if (FAILED(hr) || !dxgiDev) return; // pas d’overlay, mais pas de crash

    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&d2dFactory))) || !d2dFactory)
        return;

    if (FAILED(d2dFactory->CreateDevice(dxgiDev.Get(), &d2dDevice)) || !d2dDevice)
        return;

    if (FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dCtx)) || !d2dCtx)
        return;

    if (FAILED(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwFactory.GetAddressOf()))) || !dwFactory)
        return;

    if (FAILED(dwFactory->CreateTextFormat(
        L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        14.0f, L"en-us", &dwTextFmt)) || !dwTextFmt)
        return;

    if (FAILED(d2dCtx->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 0.95f), &dwBrush)) || !dwBrush)
        return;

    d2dReady = true; // overlay utilisable
}

void Renderer::bindD2DTargetFromSwap()
{

    if (!d2dReady || !swap) return; // rien à faire, overlay OFF

    d2dTarget.Reset();

    ComPtr<IDXGISurface> surface;
    if (FAILED(swap->GetBuffer(0, __uuidof(IDXGISurface), &surface)) || !surface)
        return;

    const D2D1_BITMAP_PROPERTIES1 props =
        D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
            96.0f, 96.0f);

    ComPtr<ID2D1Bitmap1> bmp;
    if (FAILED(d2dCtx->CreateBitmapFromDxgiSurface(surface.Get(), &props, &bmp)) || !bmp)
        return;

    d2dTarget = bmp;
    d2dCtx->SetTarget(d2dTarget.Get());
}

void Renderer::drawOverlay()
{
    if (!d2dReady || !d2dCtx) return;

    d2dCtx->BeginDraw();

    const float pad = 12.0f, panelW = 360.0f, panelH = 88.0f;
    const float vx = vp.Width, vy = vp.Height;

    const D2D1_RECT_F panel = D2D1::RectF(vx - panelW - pad, vy - panelH - pad, vx - pad, vy - pad);

    ComPtr<ID2D1SolidColorBrush> bg;
    if (SUCCEEDED(d2dCtx->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0.45f), &bg)))
        d2dCtx->FillRectangle(panel, bg.Get());

    if (dwTextFmt && dwBrush) {
        const wchar_t* text =
            L"[P] Toggle Perspective/Ortho\n"
            L"RMB: Orbit   MMB: Pan   Wheel: Zoom";
        const D2D1_RECT_F txt = D2D1::RectF(panel.left + 10.0f, panel.top + 8.0f,
            panel.right - 10.0f, panel.bottom - 8.0f);
        d2dCtx->DrawTextW(text, (UINT32)wcslen(text), dwTextFmt.Get(), txt, dwBrush.Get());
    }

    d2dCtx->EndDraw(); // on ignore l'HR: overlay best-effort
}

void Renderer::safeReleaseBackbufferBindings()
{
    // Unbind de l’OM, sinon RTV/DSV tiennent le backbuffer
    if (ctx) ctx->OMSetRenderTargets(0, nullptr, nullptr);

    // Détacher la cible D2D du backbuffer AVANT de la release
    if (d2dReady && d2dCtx) d2dCtx->SetTarget(nullptr);

    // Libérer toutes les vues liés au backbuffer
    d2dTarget.Reset();
    rtv.Reset();
    dsv.Reset();

    // S’assurer que le driver a bien relâché ses refs
    if (ctx) ctx->Flush();
}

void Renderer::handleDeviceLost()
{
    OutputDebugStringA("[Renderer] Device lost — reinitializing D3D/D2D pipeline...\n");

    // Tout démonter proprement
    safeReleaseBackbufferBindings();

    // Libérer D3D11
    rsState.Reset();
    dss.Reset();
    camCB.Reset(); objCB.Reset(); matCB.Reset();
    vs.shader.Reset(); vs.layout.Reset(); ps.shader.Reset();

    // Libérer D2D/DWrite
    d2dTarget.Reset();
    dwTextFmt.Reset();
    dwBrush.Reset();
    d2dCtx.Reset();
    d2dDevice.Reset();
    d2dFactory.Reset();
    dwFactory.Reset();
    d2dReady = false;

    // Libérer swap/device/context
    swap.Reset();
    ctx.Reset();
    dev.Reset();

    // Recréer toute la pile avec les derniers paramètres connus
    initialize(hwnd_, bbWidth_, bbHeight_);
}

void Renderer::resize(int width, int height)
{
    if (!swap) return;

    // 0) Si la fenêtre est minimisée (ou height==0), on reporte le resize
    if (width <= 0 || height <= 0) {
        // On garde au moins un viewport cohérent pour éviter divisions par 0
        vp = { 0,0, 1.0f,1.0f, 0.0f,1.0f };
        return;
    }

    bbWidth_ = width;
    bbHeight_ = height;

    // 1) Détacher et libérer toutes les refs au backbuffer
    safeReleaseBackbufferBindings();

    // 2) Tenter le ResizeBuffers SANS HR_CHECK (on gère les cas)
    HRESULT hr = swap->ResizeBuffers(
        0,                      // conserve BufferCount
        (UINT)width, (UINT)height,
        DXGI_FORMAT_UNKNOWN,    // conserve le format
        0);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        // Device lost : on repart de zéro
        handleDeviceLost();
        return;
    }
    if (FAILED(hr)) {
        // On logge et on repart sur l’ancien état (best-effort)
        OutputDebugStringA("[Renderer] ResizeBuffers failed, keeping old backbuffer.\n");
        return;
    }

    // 3) Recréer RTV/DSV
    ComPtr<ID3D11Texture2D> bb;
    HR_CHECK(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), &bb));
    HR_CHECK(dev->CreateRenderTargetView(bb.Get(), nullptr, &rtv));

    D3D11_TEXTURE2D_DESC dsd{};
    dsd.Width = width; dsd.Height = height; dsd.MipLevels = 1; dsd.ArraySize = 1;
    dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsd.SampleDesc = { 1,0 };
    dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> ds;
    HR_CHECK(dev->CreateTexture2D(&dsd, nullptr, &ds));
    HR_CHECK(dev->CreateDepthStencilView(ds.Get(), nullptr, &dsv));

    // 4) Viewport + aspect caméra
    vp = { 0,0,(FLOAT)width,(FLOAT)height, 0.0f, 1.0f };
    cam.setAspect((height > 0) ? float(width) / float(height) : 1.0f);

    // 5) Re-binder la cible D2D
    if (d2dReady) bindD2DTargetFromSwap();
}

void Renderer::render(const Scene& scene)
{
    if (vp.Width <= 0.f || vp.Height <= 0.f) return; // fenêtre minimisée

    const float bg[4] = { 0.09f, 0.09f, 0.10f, 1.0f };
    ctx->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
    ctx->ClearRenderTargetView(rtv.Get(), bg);
    ctx->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    ctx->RSSetViewports(1, &vp);

    // Pipeline
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs.shader.Get(), nullptr, 0);
    ctx->PSSetShader(ps.shader.Get(), nullptr, 0);
    ctx->IASetInputLayout(vs.layout.Get());

    // CB frame (b0 VS+PS)
    CBFrame cf{};
    XMStoreFloat4x4(&cf.View, cam.view());
    XMStoreFloat4x4(&cf.Proj, cam.proj());
    cf.LightDir[0] = -0.5f; cf.LightDir[1] = -1.0f; cf.LightDir[2] = -0.3f;
    ctx->UpdateSubresource(camCB.Get(), 0, nullptr, &cf, 0, 0);
    ctx->VSSetConstantBuffers(0, 1, camCB.GetAddressOf());
    ctx->PSSetConstantBuffers(0, 1, camCB.GetAddressOf());

    // Draw
    for (const auto& inst : scene.get()) {
        UINT stride = sizeof(float) * 6;
        UINT offset = 0;
        ctx->IASetVertexBuffers(0, 1, inst.mesh.gpu.vb.GetAddressOf(), &stride, &offset);
        ctx->IASetIndexBuffer(inst.mesh.gpu.ib.Get(), DXGI_FORMAT_R32_UINT, 0);

        CBObject co{};
        XMStoreFloat4x4(&co.World, XMLoadFloat4x4(&inst.mesh.xform.world));
        ctx->UpdateSubresource(objCB.Get(), 0, nullptr, &co, 0, 0);
        ctx->VSSetConstantBuffers(1, 1, objCB.GetAddressOf());

        CBMaterial cm{};
        cm.BaseColor[0] = inst.mesh.mat.color[0];
        cm.BaseColor[1] = inst.mesh.mat.color[1];
        cm.BaseColor[2] = inst.mesh.mat.color[2];
        ctx->UpdateSubresource(matCB.Get(), 0, nullptr, &cm, 0, 0);
        ctx->PSSetConstantBuffers(2, 1, matCB.GetAddressOf());

        ctx->DrawIndexed(inst.mesh.gpu.indexCount, 0, 0);
    }

    drawOverlay();
    swap->Present(1, 0);
}

void Renderer::toggleProjection()
{
    const float h = (vp.Height > 0.f) ? vp.Height : 1.0f;
    const float aspect = vp.Width / h;
    if (cam.mode() == ProjectionMode::Perspective) {
        cam.setOrthographic(4.0f, aspect, 0.1f, 1000.0f);
    } else {
        cam.setPerspective(DirectX::XM_PIDIV4, aspect, 0.1f, 1000.0f);
    }
}
