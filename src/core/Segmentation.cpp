// Segmentation.cpp
#include "Segmentation.h"
#include <stdexcept>
#include <algorithm>

static inline TissueLabel ClassifyHU(float hu, const SegmentationRules& r) {
    if ((int)hu >= r.bone_min) return TissueLabel::Bone;
    if ((int)hu >= r.fat_min && (int)hu <= r.fat_max) return TissueLabel::Fat;
    if ((int)hu >= r.soft_min && (int)hu <= r.soft_max) return TissueLabel::Soft;
    return TissueLabel::Background;
}

LabeledVolume SegmentVolumeHU(const VolumeData& v, const SegmentationRules& rules)
{
    const size_t N = (size_t)v.width * v.height * v.depth;
    if (v.voxels.size() != N) throw std::runtime_error("SegmentVolumeHU: taille voxels != W*H*D");

    LabeledVolume out;
    out.width = v.width; out.height = v.height; out.depth = v.depth;
    out.spacing = v.spacing; out.origin = v.origin;
    std::copy(std::begin(v.direction), std::end(v.direction), std::begin(out.direction));
    out.labels.resize(N, (uint8_t)TissueLabel::Background);

    for (size_t i = 0; i < N; ++i) {
        out.labels[i] = (uint8_t)ClassifyHU(v.voxels[i], rules);
    }
    return out;
}
