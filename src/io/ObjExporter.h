#pragma once
#include "../scene/MeshData.h"
#include <string>

// Export .obj avec positions (v), normales (vn), faces (f v//vn)
// Pas de UV/texture. Retourne true si succès.
bool ExportOBJ(const std::string& path, const MeshData& m, bool ensureNormals = true);
