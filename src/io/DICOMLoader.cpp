#include "DICOMLoader.h"

#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmimgle/dcmimage.h>
// #include <dcmtk/dcmjpeg/djdecode.h>
// #include <dcmtk/dcmdata/dcrledrg.h>

#include "../utils/MathUtils.h"
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <numeric>

namespace fs = std::filesystem;
using namespace math;

// Helpers
static inline void parseDoubles(const OFString& s, OFVector<double>& out) {
    out.clear(); std::string t(s.c_str()); size_t start = 0;
    while (start < t.size()) {
        size_t pos = t.find('\\', start);
        auto tok = t.substr(start, (pos == std::string::npos) ? std::string::npos : pos - start);
        auto b = tok.find_first_not_of(" \t\r\n"), e = tok.find_last_not_of(" \t\r\n");
        if (b != std::string::npos) out.push_back(std::stod(tok.substr(b, e - b + 1)));
        if (pos == std::string::npos) break; start = pos + 1;
    }
}

struct SliceMeta {
    std::string path;
    OFVector<double> ipp;
    double key = 0.0;
    int    inst = 0;
    // par-slice
    double slope = 1.0;
    double intercept = 0.0;
    uint16_t bitsAllocated = 16;
    uint16_t pixelRep = 0; // 0=unsigned, 1=signed
};

VolumeData DICOMLoader::loadFromDirectory(const std::string& directoryPath)
{

    std::vector<std::string> files;
    for (const auto& e : fs::directory_iterator(directoryPath)) {
        if (!e.is_regular_file()) continue;
        auto ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (ext == ".dcm") files.push_back(e.path().string());
    }
    if (files.empty()) throw std::runtime_error("Aucun DICOM dans: " + directoryPath);

    OFVector<double> refIOP, refIPP;
    uint16_t rows = 0, cols = 0;
    double pxRow = 1.0, pxCol = 1.0;

    std::vector<SliceMeta> slices; slices.reserve(files.size());
    std::string seriesUID;

    for (const auto& f : files) {
        DcmFileFormat ff; if (ff.loadFile(f.c_str()).bad()) continue;
        DcmDataset* ds = ff.getDataset(); if (!ds) continue;

        Uint16 R = 0, C = 0;
        if (ds->findAndGetUint16(DCM_Rows, R).bad() || ds->findAndGetUint16(DCM_Columns, C).bad())
            continue;

        OFString sUID;
        if (ds->findAndGetOFString(DCM_SeriesInstanceUID, sUID).good()) {
            if (seriesUID.empty()) seriesUID = sUID.c_str();
            else if (seriesUID != sUID.c_str()) continue; // ignorer autres s�ries
        }

        OFString iopS, ippS;
        if (ds->findAndGetOFStringArray(DCM_ImageOrientationPatient, iopS).bad()) continue;
        if (ds->findAndGetOFStringArray(DCM_ImagePositionPatient, ippS).bad()) continue;
        OFVector<double> iop, ipp; parseDoubles(iopS, iop); parseDoubles(ippS, ipp);
        if (iop.size() != 6 || ipp.size() != 3) continue;

        OFString psS;
        if (ds->findAndGetOFStringArray(DCM_PixelSpacing, psS).good()) {
            OFVector<double> ps; parseDoubles(psS, ps); if (ps.size() >= 2) { pxRow = ps[0]; pxCol = ps[1]; }
        }

        double slope = 1.0, intercept = 0.0;
        ds->findAndGetFloat64(DCM_RescaleSlope, slope);
        ds->findAndGetFloat64(DCM_RescaleIntercept, intercept);

        Uint16 rep = 0, ba = 16;
        ds->findAndGetUint16(DCM_PixelRepresentation, rep); // 0=unsigned, 1=signed
        ds->findAndGetUint16(DCM_BitsAllocated, ba);

        if (refIOP.empty()) { refIOP = iop; refIPP = ipp; rows = R; cols = C; }

        Sint32 inst = 0; ds->findAndGetSint32(DCM_InstanceNumber, inst);
        Vec3f r{ (float)iop[0],(float)iop[1],(float)iop[2] }, c{ (float)iop[3],(float)iop[4],(float)iop[5] };
        Vec3f n = normalize(cross(r, c));
        Vec3f d{ (float)(ipp[0] - refIPP[0]), (float)(ipp[1] - refIPP[1]), (float)(ipp[2] - refIPP[2]) };
        double key = d.x * n.x + d.y * n.y + d.z * n.z;

        slices.push_back(SliceMeta{ f, ipp, key, (int)inst, slope, intercept, ba, rep });
    }
    if (slices.empty()) throw std::runtime_error("Aucun slice DICOM exploitable.");

    std::stable_sort(slices.begin(), slices.end(), [](const SliceMeta& a, const SliceMeta& b) {
        if (a.key == b.key) return a.inst < b.inst; return a.key < b.key;
        });

    DicomImage firstImg(slices.front().path.c_str());
    if (firstImg.getStatus() != EIS_Normal || !firstImg.isMonochrome())
        throw std::runtime_error("1ere image illisible ou non monochrome: " + slices.front().path);
    const uint32_t width = firstImg.getWidth();
    const uint32_t height = firstImg.getHeight();
    const uint32_t depth = (uint32_t)slices.size();
    if (!width || !height || !depth) throw std::runtime_error("Dimensions nulles.");

    VolumeData vol;
    vol.width = width; vol.height = height; vol.depth = depth;
    vol.spacing = { (float)pxCol, (float)pxRow, 1.f };
    vol.origin = { (float)refIPP[0], (float)refIPP[1], (float)refIPP[2] };
    {
        Vec3f r{ (float)refIOP[0], (float)refIOP[1], (float)refIOP[2] };
        Vec3f c{ (float)refIOP[3], (float)refIOP[4], (float)refIOP[5] };
        Vec3f n = normalize(cross(r, c));
        setDirection(vol.direction, r, c, n);
        if (slices.size() >= 2) {
            std::vector<double> gaps; gaps.reserve(slices.size() - 1);
            for (size_t i = 1; i < slices.size(); ++i) {
                Vec3f d{ (float)(slices[i].ipp[0] - slices[i - 1].ipp[0]),
                         (float)(slices[i].ipp[1] - slices[i - 1].ipp[1]),
                         (float)(slices[i].ipp[2] - slices[i - 1].ipp[2]) };
                gaps.push_back(std::abs(d.x * n.x + d.y * n.y + d.z * n.z));
            }
            auto mid = gaps.begin() + gaps.size() / 2;
            std::nth_element(gaps.begin(), mid, gaps.end());
            vol.spacing.z = (float)(*mid > 0 ? *mid : 1.0);
        }
    }

    vol.voxels.resize((size_t)width * height * depth);
    const size_t sliceCount = (size_t)width * height;

    for (size_t z = 0; z < slices.size(); ++z) {
        DcmFileFormat ff;
        if (ff.loadFile(slices[z].path.c_str()).bad())
            throw std::runtime_error("Echec lecture: " + slices[z].path);
        DcmDataset* ds = ff.getDataset();
        if (!ds) throw std::runtime_error("Dataset nul: " + slices[z].path);

        Uint16 R = 0, C = 0;
        if (ds->findAndGetUint16(DCM_Rows, R).bad() || ds->findAndGetUint16(DCM_Columns, C).bad())
            throw std::runtime_error("Rows/Cols manquants: " + slices[z].path);
        if (R != height || C != width)
            throw std::runtime_error("Incoh�rence Rows/Cols: " + slices[z].path);

        const double slope = slices[z].slope;
        const double intercept = slices[z].intercept;
        const bool   isSigned = (slices[z].pixelRep != 0);

        DcmElement* el = nullptr;
        if (ds->findAndGetElement(DCM_PixelData, el).bad() || !el)
            throw std::runtime_error("PixelData manquant: " + slices[z].path);

        float* dst = vol.voxels.data() + z * sliceCount;

        if (slices[z].bitsAllocated <= 8) {
            // ---- 8 bits ----
            Uint8* p8 = nullptr;                             // <-- SANS const
            if (el->getUint8Array(p8).bad() || !p8)
                throw std::runtime_error("Pixels 8b indisponibles: " + slices[z].path);

            const size_t byteLen = el->getLength();          // longueur en octets
            if (byteLen < sliceCount)
                throw std::runtime_error("Taille PixelData 8b incoh�rente: " + slices[z].path);

            if (isSigned) { // int8_t
                const int8_t* s8 = reinterpret_cast<const int8_t*>(p8);
                for (size_t i = 0; i < sliceCount; ++i)
                    dst[i] = float(double(s8[i]) * slope + intercept);
            }
            else {        // uint8_t
                for (size_t i = 0; i < sliceCount; ++i)
                    dst[i] = float(double(p8[i]) * slope + intercept);
            }
        }
        else {
            // ---- 16 bits ---- 
            Uint16* p16 = nullptr;                           // <-- SANS const
            if (el->getUint16Array(p16).bad() || !p16)
                throw std::runtime_error("Pixels 16b indisponibles (codec?): " + slices[z].path);

            const size_t numVals = el->getLength() / sizeof(Uint16);
            if (numVals < sliceCount)
                throw std::runtime_error("Taille PixelData 16b incoh�rente: " + slices[z].path);

            if (isSigned) { // int16_t
                const int16_t* s16 = reinterpret_cast<const int16_t*>(p16);
                for (size_t i = 0; i < sliceCount; ++i)
                    dst[i] = float(double(s16[i]) * slope + intercept);
            }
            else {        // uint16_t
                for (size_t i = 0; i < sliceCount; ++i)
                    dst[i] = float(double(p16[i]) * slope + intercept);
            }
        }
    }


    return vol;
}
