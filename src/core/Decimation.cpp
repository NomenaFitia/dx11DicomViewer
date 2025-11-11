#include "Decimation.h"
#include "Curvature.h"
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <tuple>
#include <algorithm>
#include <numeric>
#include <tiny_obj_loader.h>
#include "../utils/MathUtils.h"

using namespace math;

static inline float length(const Vec3f& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

static double AverageEdgeLength(const MeshData& m)
{
    // moyenne des longueurs d'arêtes (chaque triangle apporte 3 arêtes)
    double acc = 0.0; size_t cnt = 0;
    for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        uint32_t a = m.indices[t], b = m.indices[t + 1], c = m.indices[t + 2];
        const Vec3f& A = m.positions[a]; const Vec3f& B = m.positions[b]; const Vec3f& C = m.positions[c];
        auto L = [&](const Vec3f& P, const Vec3f& Q) { return length({ Q.x - P.x,Q.y - P.y,Q.z - P.z }); };
        acc += L(A, B) + L(B, C) + L(C, A);
        cnt += 3;
    }
    return (cnt ? acc / double(cnt) : 0.0);
}

static inline uint64_t packKey(int x, int y, int z) {
    // pack 3 int32 en 64b (attention aux plages ; ici indices grille raisonnables)
    return ((uint64_t)(uint32_t)x << 42) | ((uint64_t)(uint32_t)y << 21) | (uint64_t)(uint32_t)z;
}

MeshData DecimateByCurvature(const MeshData& in, const DecimationSettings& s)
{
    if (in.positions.empty() || in.indices.size() < 3) return in;

    // 1) Courbure par sommet
    auto curv = EstimateVertexCurvature(in);

    // 2) Détermine les sommets "protégés" (top p% de courbure)
    const size_t V = in.positions.size();
    std::vector<uint32_t> order(V); std::iota(order.begin(), order.end(), 0u);
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return curv[a] > curv[b]; });
    size_t protectCount = (size_t)std::clamp(s.protectTopPercent, 0.f, 0.5f) * (float)V;
    std::vector<uint8_t> isProtected(V, 0);
    for (size_t i = 0; i < protectCount && i < V; ++i) isProtected[order[i]] = 1;

    // 3) Détermine la taille des cellules de clustering
    // base = moyenne des longueurs d'arêtes ; scale par factor
    double avgE = AverageEdgeLength(in);
    double cell = (avgE > 0 ? avgE : 1.0) * (1.0 + 8.0 * std::clamp(s.factor, 0.f, 1.f));
    cell *= (double)std::max(0.1f, s.cellScale);

    // 4) Bounding box pour normaliser
    Vec3f bmin = in.positions[0], bmax = in.positions[0];
    for (auto& p : in.positions) {
        bmin.x = std::min(bmin.x, p.x); bmin.y = std::min(bmin.y, p.y); bmin.z = std::min(bmin.z, p.z);
        bmax.x = std::max(bmax.x, p.x); bmax.y = std::max(bmax.y, p.y); bmax.z = std::max(bmax.z, p.z);
    }

    auto cellCoord = [&](const Vec3f& p)->std::tuple<int, int, int> {
        int gx = (int)std::floor((p.x - bmin.x) / cell);
        int gy = (int)std::floor((p.y - bmin.y) / cell);
        int gz = (int)std::floor((p.z - bmin.z) / cell);
        return { gx,gy,gz };
        };

    // 5) Mapping old->new
    //  - sommets protégés : chacun est son propre cluster (pas de fusion)
    //  - autres : cluster par cellule, repr = sommet de courbure max dans la cellule
    std::unordered_map<uint64_t, uint32_t> cellRep; // cellKey -> representative vertex id (old)
    std::vector<uint32_t> repOf(V, UINT32_MAX);
    std::vector<Vec3f> accPos; accPos.reserve(V);
    std::vector<uint32_t> accCount; accCount.reserve(V);

    // Passe 1 : choisir les représentants de cellule.
    for (uint32_t vi = 0; vi < V; ++vi) {
        if (isProtected[vi]) { repOf[vi] = vi; continue; }
        auto [gx, gy, gz] = cellCoord(in.positions[vi]);
        uint64_t key = packKey(gx, gy, gz);
        auto it = cellRep.find(key);
        if (it == cellRep.end()) {
            cellRep.emplace(key, vi);
        }
        else {
            uint32_t cur = it->second;
            if (curv[vi] > curv[cur]) it->second = vi; // garder le plus courbe
        }
    }

    // Passe 2 : assignation au représentant
    for (uint32_t vi = 0; vi < V; ++vi) {
        if (repOf[vi] != UINT32_MAX) continue; // protégé (déjà assigné à lui-même)
        auto [gx, gy, gz] = cellCoord(in.positions[vi]);
        uint64_t key = packKey(gx, gy, gz);
        auto it = cellRep.find(key);
        if (it != cellRep.end()) repOf[vi] = it->second;
        else repOf[vi] = vi; // fallback sécurité
    }

    // 6) Construire les nouveaux sommets : id compacts + position moyenne par cluster (ou repr)
    std::unordered_map<uint32_t, uint32_t> newIndexOfRep; // oldRep -> newIndex
    MeshData out;
    out.positions.reserve(V);

    accPos.resize(V, { 0,0,0 }); accCount.resize(V, 0);
    for (uint32_t vi = 0; vi < V; ++vi) {
        uint32_t rep = repOf[vi];
        accPos[rep].x += in.positions[vi].x;
        accPos[rep].y += in.positions[vi].y;
        accPos[rep].z += in.positions[vi].z;
        accCount[rep] += 1;
    }
    for (auto& kv : cellRep) {
        uint32_t rep = kv.second;
        if (newIndexOfRep.find(rep) == newIndexOfRep.end()) {
            uint32_t newId = (uint32_t)out.positions.size();
            newIndexOfRep.emplace(rep, newId);
            // position = moyenne des points du cluster (plus stable que le repr seul)
            float inv = 1.0f / std::max(1u, accCount[rep]);
            out.positions.push_back({ accPos[rep].x * inv, accPos[rep].y * inv, accPos[rep].z * inv });
        }
    }
    // Ajoute aussi les protégés qui n'étaient pas dans cellRep (rare)
    for (uint32_t vi = 0; vi < V; ++vi) if (isProtected[vi]) {
        if (newIndexOfRep.find(vi) == newIndexOfRep.end()) {
            uint32_t newId = (uint32_t)out.positions.size();
            newIndexOfRep.emplace(vi, newId);
            out.positions.push_back(in.positions[vi]);
        }
    }

    // 7) Reconstruire faces en mappant chaque sommet vers l'id compact
    auto mapToNew = [&](uint32_t vi)->uint32_t {
        uint32_t rep = repOf[vi];
        auto it = newIndexOfRep.find(rep);
        return (it != newIndexOfRep.end()) ? it->second : (uint32_t)-1;
        };

    out.indices.reserve(in.indices.size());
    std::unordered_set<uint64_t> faceSet; faceSet.reserve(in.indices.size());

    auto faceKey = [](uint32_t a, uint32_t b, uint32_t c)->uint64_t {
        // clé canonique triée pour éviter doublons (a,b,c < 2^21)
        uint32_t v[3] = { a,b,c }; std::sort(v, v + 3);
        return ((uint64_t)v[0] << 42) | ((uint64_t)v[1] << 21) | (uint64_t)v[2];
        };

    for (size_t t = 0; t + 2 < in.indices.size(); t += 3) {
        uint32_t a = mapToNew(in.indices[t]);
        uint32_t b = mapToNew(in.indices[t + 1]);
        uint32_t c = mapToNew(in.indices[t + 2]);
        if (a == b || b == c || c == a) continue; // triangle dégénéré par fusion

        // rejeter triangles à aire ~0
        const Vec3f& A = out.positions[a]; const Vec3f& B = out.positions[b]; const Vec3f& C = out.positions[c];
        Vec3f n = cross({ B.x - A.x,B.y - A.y,B.z - A.z }, { C.x - A.x,C.y - A.y,C.z - A.z });
        if (length(n) < 1e-9f) continue;

        uint64_t k = faceKey(a, b, c);
        if (faceSet.insert(k).second) {
            out.indices.push_back(a); out.indices.push_back(b); out.indices.push_back(c);
        }
    }

    // 8) Normales propres
    RecomputeVertexNormals(out);
    return out;
}

/*
Ce décimateur évite toute chirurgie de topologie complexe (edge collapse) tout en donnant 
un résultat propre et préservant la haute courbure (le détail), ce qui est souvent ce qu’on
veut avant des exports/démos.
Tu peux ajuster protectTopPercent (par ex. 0.05) et cellScale pour peaufiner la qualité.
*/