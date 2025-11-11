#pragma once
#include "../scene/MeshData.h"
#include <cstdint>

struct DecimationSettings {
    // facteur dans [0..1] : 0 = pas de décimation, 1 = très agressif
    float factor = 0.5f;
    // pourcentage de sommets à "protéger" (haute courbure) [0..0.5]
    float protectTopPercent = 0.10f;
    // multiplier sur la taille de cellule de base (affine l'échelle)
    float cellScale = 1.0f;
};

// Décime un mesh en préservant la haute courbure (clustering + protection).
// Recalcule des normales propres à la fin. Supprime les faces dégénérées et doublons.
MeshData DecimateByCurvature(const MeshData& in, const DecimationSettings& s);
