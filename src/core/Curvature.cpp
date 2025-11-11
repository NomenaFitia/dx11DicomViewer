#include "Curvature.h"
#include <cmath>
#include <algorithm>
#include <numeric>

static inline Vec3f cross(const Vec3f& a, const Vec3f& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
static inline float dot(const Vec3f& a, const Vec3f& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline float norm(const Vec3f& v) { return std::sqrt(std::max(0.f, dot(v, v))); }
static inline Vec3f normalize(const Vec3f& v) {
    float n = norm(v); return (n > 1e-12f) ? Vec3f{ v.x / n,v.y / n,v.z / n } : Vec3f{ 0,0,1 };
}

void RecomputeVertexNormals(MeshData& m)
{
    m.normals.assign(m.positions.size(), { 0,0,0 });
    const auto& I = m.indices;
    for (size_t t = 0; t + 2 < I.size(); t += 3) {
        uint32_t a = I[t], b = I[t + 1], c = I[t + 2];
        const Vec3f& A = m.positions[a]; const Vec3f& B = m.positions[b]; const Vec3f& C = m.positions[c];
        Vec3f n = cross({ B.x - A.x,B.y - A.y,B.z - A.z }, { C.x - A.x,C.y - A.y,C.z - A.z });
        // aire ~ |n|/2 ; on accumule la normale pondérée par aire (|n|)
        m.normals[a] = { m.normals[a].x + n.x, m.normals[a].y + n.y, m.normals[a].z + n.z };
        m.normals[b] = { m.normals[b].x + n.x, m.normals[b].y + n.y, m.normals[b].z + n.z };
        m.normals[c] = { m.normals[c].x + n.x, m.normals[c].y + n.y, m.normals[c].z + n.z };
    }
    for (auto& n : m.normals) n = normalize(n);
}

std::vector<float> EstimateVertexCurvature(const MeshData& m)
{
    
    MeshData tmp = m;
    if (tmp.normals.size() != tmp.positions.size()) RecomputeVertexNormals(tmp);

    const size_t V = tmp.positions.size();
    std::vector<float> curv(V, 0.f);
    std::vector<uint32_t> deg(V, 0);

    std::vector<Vec3f> fn(m.indices.size() / 3);
    for (size_t t = 0; t < fn.size(); ++t) {
        uint32_t a = m.indices[3 * t + 0], b = m.indices[3 * t + 1], c = m.indices[3 * t + 2];
        const Vec3f& A = m.positions[a]; const Vec3f& B = m.positions[b]; const Vec3f& C = m.positions[c];
        fn[t] = normalize(cross({ B.x - A.x,B.y - A.y,B.z - A.z }, { C.x - A.x,C.y - A.y,C.z - A.z }));
    }

    // accumulate |angle(Nv, Nf)|
    for (size_t t = 0; t < fn.size(); ++t) {
        uint32_t a = m.indices[3 * t + 0], b = m.indices[3 * t + 1], c = m.indices[3 * t + 2];
        auto add = [&](uint32_t v) {
            float d = std::clamp(dot(tmp.normals[v], fn[t]), -1.0f, 1.0f);
            float ang = std::acos(d);
            curv[v] += ang;
            deg[v] += 1;
            };
        add(a); add(b); add(c);
    }
    for (size_t i = 0; i < V; ++i) if (deg[i]) curv[i] /= float(deg[i]); // moyenne des angles
    return curv; // plus grand => haute courbure
}
