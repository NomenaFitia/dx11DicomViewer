#pragma once
#include <vector>
#include "Types.h"

// Volume en HU (pr�t pour segmentation/isosurface)
struct VolumeData
{
    uint32_t width = 0, height = 0, depth = 0;     // Columns, Rows, #slices
    Vec3f spacing{ 1,1,1 };                        // mm : x=col, y=row, z=slice gap
    Vec3f origin{ 0,0,0 };                         // IPP du 1er slice tri�
    float direction[9] = { 1,0,0, 0,1,0, 0,0,1 };   // (row, col, normal)

    // Voxels : Hounsfield Units directement
    std::vector<float> voxels;                     // size = W*H*D, en HU
};

enum class TissueLabel : uint8_t { Background = 0, Bone = 1, Soft = 2, Fat = 3 };

struct LabeledVolume
{
    uint32_t width = 0, height = 0, depth = 0;
    Vec3f spacing{ 1,1,1 };
    Vec3f origin{ 0,0,0 };
    float direction[9] = { 1,0,0, 0,1,0, 0,0,1 };
    std::vector<uint8_t> labels;                  // TissueLabel par voxel (0/1/2/3)
};
