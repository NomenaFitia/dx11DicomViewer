#pragma once
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <numeric>
#include "../core/Types.h" // ou "core/Types.h" si Vec3f y est déclaré

namespace math {

    static inline Vec3f cross(const Vec3f& a, const Vec3f& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    static inline float dot(const Vec3f& a, const Vec3f& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static inline float norm(const Vec3f& v) { return std::sqrt(dot(v, v)); }
    static inline Vec3f normalize(const Vec3f& v) { float n = norm(v); return (n > 0.f) ? Vec3f{ v.x / n, v.y / n, v.z / n } : Vec3f{ 0,0,1 }; }
    static inline void setDirection(float M[9], const Vec3f& r, const Vec3f& c, const Vec3f& n) {
        M[0] = r.x; M[1] = r.y; M[2] = r.z; M[3] = c.x; M[4] = c.y; M[5] = c.z; M[6] = n.x; M[7] = n.y; M[8] = n.z;
    }

} // namespace math
