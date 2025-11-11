#pragma once
#include "../scene/MeshData.h"
#include <vector>

// Calcule des normales par sommet (pondération aire des faces)
void RecomputeVertexNormals(MeshData& m);

// Proxy de courbure par sommet : moyenne des écarts angulaires entre la normale du sommet
// et les normales de ses faces adjacentes. Unité ~ radians (bornée [0..pi]).
std::vector<float> EstimateVertexCurvature(const MeshData& m);
