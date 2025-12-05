#include "IsoSurfaceMC.h"
#include <cmath>
#include <array>
#include "tables.h"
#include <iostream>
#include <unordered_map>

using namespace std;

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
        auto v = static_cast<TissueLabel>(vol.labels[idx3D(x, y, z, vol.width, vol.height, vol.depth)]);
        return (v == label) ? 1u : 0u;
    }

    static inline void loadDir3x3(const float d[9], float M[3][3])
    {
        M[0][0] = d[0]; M[0][1] = d[1]; M[0][2] = d[2];
        M[1][0] = d[3]; M[1][1] = d[4]; M[1][2] = d[5];
        M[2][0] = d[6]; M[2][1] = d[7]; M[2][2] = d[8];
    }

    static inline Vec3f voxelToWorld(const LabeledVolume& vol, float i, float j, float k)
    {
        float M[3][3]; loadDir3x3(vol.direction, M);
        const Vec3f s = vol.spacing;
        const float vx = i * s.x, vy = j * s.y, vz = k * s.z;
        return {
            vol.origin.x + M[0][0] * vx + M[0][1] * vy + M[0][2] * vz,
            vol.origin.y + M[1][0] * vx + M[1][1] * vy + M[1][2] * vz,
            vol.origin.z + M[2][0] * vx + M[2][1] * vy + M[2][2] * vz
        };
    }

    static inline Vec3f sub(const Vec3f& a, const Vec3f& b) { return { a.x - b.x,a.y - b.y,a.z - b.z }; }
    static inline Vec3f cross(const Vec3f& a, const Vec3f& b)
    {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }
    static inline float  norm(const Vec3f& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
    static inline Vec3f  normalize(const Vec3f& v) { float n = norm(v); return (n > 1e-20f) ? Vec3f{ v.x / n,v.y / n,v.z / n } : Vec3f{ 0,0,0 }; }

    static inline Vec3f vertexInterp(float iso, const Vec3f& p1, const Vec3f& p2, float v1, float v2)
    {
        if (std::abs(iso - v1) < 1e-6f) return p1;
        if (std::abs(iso - v2) < 1e-6f) return p2;
        if (std::abs(v1 - v2) < 1e-12f) return p1;
        float mu = (iso - v1) / (v2 - v1);
        return { p1.x + mu * (p2.x - p1.x), p1.y + mu * (p2.y - p1.y), p1.z + mu * (p2.z - p1.z) };
    }

    // Gradient du binaire “catégorie=1, autre=0” (pointe vers l’intérieur de la catégorie)
    static inline Vec3f gradientAtWorld(const LabeledVolume& vol, int x, int y, int z, TissueLabel category)
    {
        auto s = [&](int X, int Y, int Z)->float {
            if (!inBounds(X, Y, Z, vol.width, vol.height, vol.depth)) return 0.f;
            return getBinary(vol, X, Y, Z, category) ? 1.f : 0.f;
            };
        const float gx = 0.5f * (s(x + 1, y, z) - s(x - 1, y, z));
        const float gy = 0.5f * (s(x, y + 1, z) - s(x, y - 1, z));
        const float gz = 0.5f * (s(x, y, z + 1) - s(x, y, z - 1));

        float M[3][3]; loadDir3x3(vol.direction, M);
        const Vec3f sp = vol.spacing;
        const float vx = gx / sp.x, vy = gy / sp.y, vz = gz / sp.z; // echelle
        return {
            M[0][0] * vx + M[0][1] * vy + M[0][2] * vz,
            M[1][0] * vx + M[1][1] * vy + M[1][2] * vz,
            M[2][0] * vx + M[2][1] * vy + M[2][2] * vz
        };
    }

    MeshData marchingcubes(const LabeledVolume& vol, TissueLabel category)
    {
        MeshData mesh;
        const uint32_t W = vol.width, H = vol.height, D = vol.depth;
        if (W < 2 || H < 2 || D < 2) return mesh;

        // éviter “caps” sur bords de slices, tout en restant safe pour D<=2
        const uint32_t kStart = (D > 2) ? 1 : 0;
        const uint32_t kEnd = (D > 2) ? (D - 1) : (D - 1); // on itère jusqu’à D-2 inclus (condition z<kEnd ci-dessous)

        // CHAMP SCALAIRE : 0 = “inside catégorie”, 1 = “outside”
        auto field = [&](int x, int y, int z)->float {
            return getBinary(vol, x, y, z, category) ? 0.f : 1.f;
            };
        const float isovalue = 0.5f;

        // cache des sommets par arête (réduction des doublons + normales lissées)
        struct EdgeKey {
            uint32_t x, y, z; uint8_t e; bool operator==(const EdgeKey& o)const noexcept {
                return x == o.x && y == o.y && z == o.z && e == o.e;
            }
        };
        struct EdgeKeyHash {
            size_t operator()(const EdgeKey& k)const noexcept {
                size_t h = k.x; h = (h * 1315423911u) ^ k.y; h = (h * 1315423911u) ^ k.z; h = (h * 1315423911u) ^ k.e; return h;
            }
        };

        std::unordered_map<EdgeKey, uint32_t, EdgeKeyHash> edgeCache; edgeCache.reserve((size_t)W * H);

        auto getOrCreate = [&](uint32_t cx, uint32_t cy, uint32_t cz, uint8_t e, const Vec3f& pos)->uint32_t {
            EdgeKey k{ cx,cy,cz,e }; auto it = edgeCache.find(k);
            if (it != edgeCache.end()) return it->second;
            uint32_t id = (uint32_t)mesh.positions.size();
            mesh.positions.push_back(pos);
            mesh.normals.push_back({ 0,0,0 }); // placeholder
            edgeCache.emplace(k, id);
            return id;
            };

        auto addFace = [&](uint32_t ia, uint32_t ib, uint32_t ic, int vx, int vy, int vz) {
            // normale de face (aire-pondérée)
            const Vec3f A = mesh.positions[ia], B = mesh.positions[ib], C = mesh.positions[ic];
            Vec3f n = cross(sub(B, A), sub(C, A));
            if (norm(n) < 1e-15f) return; // dégénéré

            // orienter la face pour que la normale pointe VERS L’EXTÉRIEUR de la catégorie
            // gradient(binaire catégorie) pointe VERS L’INTÉRIEUR → si dot(n,grad)>0, on flippe
            Vec3f g = gradientAtWorld(vol, vx, vy, vz, category);
            if ((n.x * g.x + n.y * g.y + n.z * g.z) < 0.f) { // corrieger les normales
                // flip winding
                std::swap(ib, ic);
                n = cross(sub(mesh.positions[ib], A), sub(mesh.positions[ic], A));
                if (norm(n) < 1e-15f) return;
            }

            mesh.indices.push_back(ia); mesh.indices.push_back(ib); mesh.indices.push_back(ic);
            // accumulate
            mesh.normals[ia] = { mesh.normals[ia].x + n.x, mesh.normals[ia].y + n.y, mesh.normals[ia].z + n.z };
            mesh.normals[ib] = { mesh.normals[ib].x + n.x, mesh.normals[ib].y + n.y, mesh.normals[ib].z + n.z };
            mesh.normals[ic] = { mesh.normals[ic].x + n.x, mesh.normals[ic].y + n.y, mesh.normals[ic].z + n.z };
            };

        for (uint32_t z = kStart; z + 1 < kEnd; ++z)           // z in [kStart .. D-2] si D>2
            for (uint32_t y = 0; y + 1 < H; ++y)
                for (uint32_t x = 0; x + 1 < W; ++x)
                {
                    // valeurs du champ (0 inside, 1 outside)
                    float V[8] = {
                        field((int)x,(int)y,(int)z),     field((int)x + 1,(int)y,(int)z),
                        field((int)x + 1,(int)y + 1,(int)z), field((int)x,(int)y + 1,(int)z),
                        field((int)x,(int)y,(int)z + 1),   field((int)x + 1,(int)y,(int)z + 1),
                        field((int)x + 1,(int)y + 1,(int)z + 1), field((int)x,(int)y + 1,(int)z + 1)
                    };

                    int cubeIndex = 0;
                    if (V[0] < isovalue) cubeIndex |= 1;
                    if (V[1] < isovalue) cubeIndex |= 2;
                    if (V[2] < isovalue) cubeIndex |= 4;
                    if (V[3] < isovalue) cubeIndex |= 8;
                    if (V[4] < isovalue) cubeIndex |= 16;
                    if (V[5] < isovalue) cubeIndex |= 32;
                    if (V[6] < isovalue) cubeIndex |= 64;
                    if (V[7] < isovalue) cubeIndex |= 128;

                    const int eMask = edgeTable[cubeIndex];
                    if (!eMask) continue;

                    // positions monde des 8 coins
                    Vec3f P[8] = {
                        voxelToWorld(vol, x    , y    , z),
                        voxelToWorld(vol, x + 1.f, y    , z),
                        voxelToWorld(vol, x + 1.f, y + 1.f, z),
                        voxelToWorld(vol, x    , y + 1.f, z),
                        voxelToWorld(vol, x    , y    , z + 1.f),
                        voxelToWorld(vol, x + 1.f, y    , z + 1.f),
                        voxelToWorld(vol, x + 1.f, y + 1.f, z + 1.f),
                        voxelToWorld(vol, x    , y + 1.f, z + 1.f)
                    };

                    // intersections par arête
                    Vec3f E[12];
                    if (eMask & 1)    E[0] = vertexInterp(isovalue, P[0], P[1], V[0], V[1]);
                    if (eMask & 2)    E[1] = vertexInterp(isovalue, P[1], P[2], V[1], V[2]);
                    if (eMask & 4)    E[2] = vertexInterp(isovalue, P[2], P[3], V[2], V[3]);
                    if (eMask & 8)    E[3] = vertexInterp(isovalue, P[3], P[0], V[3], V[0]);
                    if (eMask & 16)   E[4] = vertexInterp(isovalue, P[4], P[5], V[4], V[5]);
                    if (eMask & 32)   E[5] = vertexInterp(isovalue, P[5], P[6], V[5], V[6]);
                    if (eMask & 64)   E[6] = vertexInterp(isovalue, P[6], P[7], V[6], V[7]);
                    if (eMask & 128)  E[7] = vertexInterp(isovalue, P[7], P[4], V[7], V[4]);
                    if (eMask & 256)  E[8] = vertexInterp(isovalue, P[0], P[4], V[0], V[4]);
                    if (eMask & 512)  E[9] = vertexInterp(isovalue, P[1], P[5], V[1], V[5]);
                    if (eMask & 1024) E[10] = vertexInterp(isovalue, P[2], P[6], V[2], V[6]);
                    if (eMask & 2048) E[11] = vertexInterp(isovalue, P[3], P[7], V[3], V[7]);

                    // triangles
                    for (int t = 0; triTable[cubeIndex][t] != -1; t += 3)
                    {
                        const int ea = triTable[cubeIndex][t];
                        const int eb = triTable[cubeIndex][t + 1];
                        const int ec = triTable[cubeIndex][t + 2];

                        const uint32_t ia = getOrCreate(x, y, z, (uint8_t)ea, E[ea]);
                        const uint32_t ib = getOrCreate(x, y, z, (uint8_t)eb, E[eb]);
                        const uint32_t ic = getOrCreate(x, y, z, (uint8_t)ec, E[ec]);

                        // vx,vy,vz ~ cellule courante pour l’orientation via gradient
                        addFace(ia, ib, ic, (int)x, (int)y, (int)z);
                    }
                }

        // normalisation finale des normales sommées
        for (auto& n : mesh.normals) n = normalize(n);

        // si rien n’a été généré, on garantit cohérence des vecteurs
        if (mesh.indices.empty()) { mesh.positions.clear(); mesh.normals.clear(); }

        return mesh;
    }

} // namespace core

