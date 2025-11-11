#pragma once
#include <string>
#include "../core/Volume.h"  //  VolumeData, LabeledVolume

class DICOMLoader
{
public:
    // Charge une série DICOM depuis un dossier (une série unique ; les autres séries sont ignorées)
    // - Tri des slices par projection IPP sur la normale issue de IOP
    // - spacing.x,y depuis PixelSpacing ; spacing.z = médiane des gaps
    // - direction = [row; col; normal]
    // - voxels : valeurs brutes en uint16 (getOutputData(16) de DicomImage)
    VolumeData loadFromDirectory(const std::string& directoryPath);
};
