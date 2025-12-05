#pragma once
#include "../scene/MeshData.h"
#include <vector>

// Calcule des normales par sommet (ponderation aire des faces)
void RecomputeVertexNormals(MeshData& m);

// Proxy de courbure par sommet : moyenne des ecarts angulaires entre la normale du sommet
// et les normales de ses faces adjacentes. Unite ~ radians (bornee [0..pi]).
std::vector<float> EstimateVertexCurvature(const MeshData& m);
