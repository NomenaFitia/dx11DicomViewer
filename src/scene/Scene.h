#pragma once
#include "Mesh.h"
#include <vector>


class Scene {
public:
	struct Instance {
		Mesh mesh;
	};


	void clear() { instances.clear(); }
	void add(Instance&& inst) { instances.emplace_back(std::move(inst)); }


	const std::vector<Instance>& get() const { return instances; }


private:
	std::vector<Instance> instances;
};