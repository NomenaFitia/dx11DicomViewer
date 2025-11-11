#pragma once
#include "Volume.h"

struct SegmentationRules {
    int bone_min = 300;
    int soft_min = -29;
    int soft_max = 150;
    int fat_min = -190;
    int fat_max = -30;
};

LabeledVolume SegmentVolumeHU(const VolumeData& v, const SegmentationRules& rules = {});
