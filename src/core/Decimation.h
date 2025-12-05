#pragma once
#include "../scene/MeshData.h"
#include <cstdint>

struct DecimationSettings {
    // facteur dans [0..1] : 0 = pas de decimation, 1 = tres agressif
    float factor = 0.5f;
    // pourcentage de sommets a "proteger" (haute courbure) [0..0.5]
    float protectTopPercent = 0.10f;
    // multiplier sur la taille de cellule de base (affine l'echelle)
    float cellScale = 1.0f;
};

MeshData DecimateByCurvature(const MeshData& in, const DecimationSettings& s);
