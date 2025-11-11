#pragma once
#include <vector>
#include <cstdint>
#include "../core/Types.h"

struct MeshData {
	std::vector<Vec3f> positions;
	std::vector<Vec3f> normals;
	std::vector<uint32_t> indices; // triangles i0,i1,i2
};