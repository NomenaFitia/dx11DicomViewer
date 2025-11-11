#pragma once
#include "../platform/Window.h"
#include "../render/Renderer.h"
#include "../scene/Scene.h"
#include "../utils/Timer.h"
#include "../utils/Color.h"


class App {
public:
	void run();


	// API publique pour charger des MeshData externes + couleur (optionnelle)
	void addMesh(const MeshData& data, const Color3f* color = nullptr);

	std::function<void()> onInitialized = nullptr;

private:
	struct Pending {
		MeshData data;
		std::optional<Color3f> color;
	};

	void flushPending();

	Window wnd;
	Renderer renderer;
	Scene scene;
	Timer timer;

	bool ready_ = false;
	std::vector<Pending> pending_;
};