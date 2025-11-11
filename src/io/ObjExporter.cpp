#include "OBJExporter.h"
#include "../core/Curvature.h"
#include <fstream>
#include <iomanip>

bool ExportOBJ(const std::string& path, const MeshData& m, bool ensureNormals)
{
    if (m.indices.size() < 3 || m.positions.empty()) return false;

    MeshData tmp = m;
    if (ensureNormals && (tmp.normals.size() != tmp.positions.size()))
        RecomputeVertexNormals(tmp);

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out << std::fixed << std::setprecision(6);

    // v
    for (const auto& p : tmp.positions)
        out << "v " << p.x << " " << p.y << " " << p.z << "\n";

    // vn
    if (!tmp.normals.empty()) {
        for (const auto& n : tmp.normals)
            out << "vn " << n.x << " " << n.y << " " << n.z << "\n";
    }

    // f (1-based indexing ; "v//vn" sans vt)
    const bool hasN = (tmp.normals.size() == tmp.positions.size());
    for (size_t t = 0; t + 2 < tmp.indices.size(); t += 3) {
        uint32_t a = tmp.indices[t] + 1, b = tmp.indices[t + 1] + 1, c = tmp.indices[t + 2] + 1;
        if (hasN) out << "f " << a << "//" << a << " " << b << "//" << b << " " << c << "//" << c << "\n";
        else      out << "f " << a << " " << b << " " << c << "\n";
    }
    return true;
}
