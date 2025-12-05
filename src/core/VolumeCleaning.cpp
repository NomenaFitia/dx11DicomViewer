#include "VolumeCleaning.h"
#include <unordered_map>

namespace core {

    static inline uint32_t idx3D(uint32_t x, uint32_t y, uint32_t z,
        uint32_t W, uint32_t H, uint32_t /*D*/)
    {
        return z * (W * H) + y * W + x;
    }

    static inline bool inBounds(int x, int y, int z, int W, int H, int D)
    {
        return (x >= 0 && x < W) && (y >= 0 && y < H) && (z >= 0 && z < D);
    }

    static inline uint8_t getBinary(const LabeledVolume& vol, uint32_t x, uint32_t y, uint32_t z, TissueLabel label)
    {
        const auto v = static_cast<TissueLabel>(vol.labels[idx3D(x, y, z, vol.width, vol.height, vol.depth)]);
        return (v == label) ? 1u : 0u;
    }

    static std::vector<std::array<int, 3>> MakeCubeStencil(int radius)
    {
        std::vector<std::array<int, 3>> off;
        off.reserve((2 * radius + 1) * (2 * radius + 1) * (2 * radius + 1));
        for (int dz = -radius; dz <= radius; ++dz)
            for (int dy = -radius; dy <= radius; ++dy)
                for (int dx = -radius; dx <= radius; ++dx)
                    off.push_back({ dx,dy,dz });
        return off;
    }

    static void ErodeBinaryView(const LabeledVolume& vol,
        const std::vector<uint8_t>* srcBits, TissueLabel L,
        int radius, std::vector<uint8_t>& out)
    {
        const int W = (int)vol.width, H = (int)vol.height, D = (int)vol.depth;
        out.assign((size_t)W * H * D, 0u);
        auto stencil = MakeCubeStencil(radius);

        auto at = [&](int x, int y, int z)->uint8_t {
            if (!inBounds(x, y, z, W, H, D)) return 0u;
            if (srcBits) return (*srcBits)[idx3D(x, y, z, vol.width, vol.height, vol.depth)];
            return getBinary(vol, x, y, z, L);
            };

        for (int z = 0; z < D; ++z)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    uint8_t keep = 1u;
                    for (auto& o : stencil)
                    {
                        if (!at(x + o[0], y + o[1], z + o[2])) { keep = 0u; break; }
                    }
                    out[idx3D(x, y, z, vol.width, vol.height, vol.depth)] = keep;
                }
    }

    static void DilateBinaryView(const LabeledVolume& vol,
        const std::vector<uint8_t>* srcBits, TissueLabel L,
        int radius, std::vector<uint8_t>& out)
    {
        const int W = (int)vol.width, H = (int)vol.height, D = (int)vol.depth;
        out.assign((size_t)W * H * D, 0u);
        auto stencil = MakeCubeStencil(radius);

        auto at = [&](int x, int y, int z)->uint8_t {
            if (!inBounds(x, y, z, W, H, D)) return 0u;
            if (srcBits) return (*srcBits)[idx3D(x, y, z, vol.width, vol.height, vol.depth)];
            return getBinary(vol, x, y, z, L);
            };

        for (int z = 0; z < D; ++z)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    uint8_t any = 0u;
                    for (auto& o : stencil)
                    {
                        if (at(x + o[0], y + o[1], z + o[2])) { any = 1u; break; }
                    }
                    out[idx3D(x, y, z, vol.width, vol.height, vol.depth)] = any;
                }
    }

    LabeledVolume& CleanLabeledVolume(LabeledVolume& vol, const MorphologyParams& p)
    {
        if (vol.labels.empty() || vol.width * vol.height * vol.depth != vol.labels.size())
            return vol;

        // On traite uniquement Bone/Soft/Fat
        std::array<TissueLabel, 3> classes = { TissueLabel::Bone, TissueLabel::Soft, TissueLabel::Fat };

        // binaire nettoye par classe
        std::unordered_map<int, std::vector<uint8_t>> cleaned;
        cleaned.reserve(classes.size());

        std::vector<uint8_t> tmpA, tmpB, current;

        for (auto L : classes)
        {
            // Erosion
            ErodeBinaryView(vol, nullptr, L, p.radius, tmpA);
            current = tmpA;
            for (int it = 1; it < p.iterations; ++it) {
                ErodeBinaryView(vol, &current, L, p.radius, tmpB);
                current.swap(tmpB);
            }
            // Dilatation
            for (int it = 0; it < p.iterations; ++it) {
                DilateBinaryView(vol, &current, L, p.radius, tmpB);
                current.swap(tmpB);
            }

            if (p.do_open_close)
            {
                for (int it = 0; it < p.iterations; ++it) {
                    DilateBinaryView(vol, &current, L, p.radius, tmpA);
                    current.swap(tmpA);
                }
                for (int it = 0; it < p.iterations; ++it) {
                    ErodeBinaryView(vol, &current, L, p.radius, tmpA);
                    current.swap(tmpA);
                }
            }

            cleaned[(int)L] = std::move(current);
            current.clear();
            tmpA.clear();
            tmpB.clear();
        }

        std::fill(vol.labels.begin(), vol.labels.end(), (uint8_t)TissueLabel::Background);
        const uint32_t W = vol.width, H = vol.height, D = vol.depth;
        for (auto L : p.priority)
        {
            if (L == TissueLabel::Background) continue;
            auto it = cleaned.find((int)L);
            if (it == cleaned.end()) continue;
            const auto& bin = it->second;

            for (uint32_t z = 0; z < D; ++z)
                for (uint32_t y = 0; y < H; ++y)
                    for (uint32_t x = 0; x < W; ++x)
                    {
                        auto id = idx3D(x, y, z, W, H, D);
                        if (bin[id] && vol.labels[id] == (uint8_t)TissueLabel::Background)
                            vol.labels[id] = (uint8_t)L;
                    }
        }

        return vol;
    }

} // namespace core
