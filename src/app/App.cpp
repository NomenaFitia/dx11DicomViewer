#include "App.h"
#include "../utils/Color.h"
#include <DirectXMath.h>
#include <fmt/core.h>
using namespace DirectX;


void App::addMesh(const MeshData& data, const Color3f* color)
{
	// Si le device n'est pas prêt, on empile et on fera l'upload après init
	if (!ready_ || renderer.device() == nullptr) {
		Pending p{ data, std::nullopt };
		if (color) p.color = *color;
		pending_.push_back(std::move(p));
		return;
	}

	Scene::Instance inst{};
	inst.mesh = MeshFactory::CreateFrom(renderer.device(), data);
	inst.mesh.mat.color[0] = color->r; inst.mesh.mat.color[1] = color->g; inst.mesh.mat.color[2] = color->b;

	// World = identité
	scene.add(std::move(inst));
}

void App::flushPending()
{
	if (!ready_ || renderer.device() == nullptr) return;

	auto pendingCopy = std::move(pending_);
	pending_.clear();
	for (auto& p : pendingCopy)
		addMesh(p.data, p.color ? &*p.color : nullptr);
}


void App::run()
{
	const int W = 1280, H = 720;
	wnd.create(L"Dx11MeshViewer", W, H);
	renderer.initialize(wnd.hwnd(), W, H);

	if (onInitialized) onInitialized();   // <-- device prêt, addMesh() créera les buffers

	// Camera controls (RMB rotate, MMB pan, wheel zoom)
	wnd.setOnResize([this](int w, int h) { renderer.resize(w, h); });
	wnd.setOnMouseDrag([this](float dx, float dy, int btn) {
		if (btn == 1) renderer.camera().arc(dx * 0.005f, dy * 0.005f); // RMB
		if (btn == 2) renderer.camera().pan(dx * 0.01f, dy * 0.01f); // MMB
		});
	wnd.setOnWheel([this](float steps) { renderer.camera().dolly(steps * 0.08f); });
	
	// toggle perspective/ortho with 'P'
	wnd.onKeyDown = [this](UINT key) {
		if (key == 'P') renderer.toggleProjection();
	};

	wnd.show();

	ready_ = true;
	flushPending();

	while (wnd.pump()) {
		renderer.render(scene);
	}
}