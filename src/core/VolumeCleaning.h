#pragma once
#include "Volume.h"
#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "Types.h"  // Vec3f, TissueLabel, LabeledVolume

namespace core {

    struct MorphologyParams
    {
        // Rayon (voxels) du SE cubique : 1 => 3x3x3, 2 => 5x5x5, etc.
        int radius = 1;

        // Nb d'itérations pour chaque passe (utile si bruit tenace).
        int iterations = 1;

        // Chaîne ouverture puis fermeture (erosion->dilatation, puis dilatation->erosion)
        bool do_open_close = true;

        // Priorité de recomposition quand plusieurs classes se recouvrent après nettoyage
        // (plus haut = plus prioritaire).
        std::array<TissueLabel, 4> priority = {
            TissueLabel::Bone, TissueLabel::Soft, TissueLabel::Fat, TissueLabel::Background
        };
    };

    // Nettoie les labels en place et retourne la même instance (chaînable).
    LabeledVolume& CleanLabeledVolume(LabeledVolume& vol, const MorphologyParams& p = {});

} // namespace core
