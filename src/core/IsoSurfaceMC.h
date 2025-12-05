#pragma once
#include <vector>
#include "Types.h"          // TissueLabel, LabeledVolume, Vec3f
#include "../scene/MeshData.h" // MeshData { positions, normals, indices }
#include "Volume.h"        // LabeledVolume

namespace core {

	
	MeshData marchingcubes(const LabeledVolume& vol, TissueLabel category);

	// Extrait un mesh par classe utile (Bone/Soft/Fat), dans cet ordre.
	std::vector<MeshData> extractMeshes(const LabeledVolume& vol);

} // namespace core
