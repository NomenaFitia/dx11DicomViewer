Cette version lit les pixels via DicomImage::getOutputData(16) → c’est simple et compatible avec les séries compressées (si les modules dcmjpeg/dcmimgle sont bien liés).

On trie les slices par projection d’IPP sur la normale issue d’IOP (plus robuste que SliceLocation/InstanceNumber).

spacing.x = PixelSpacing[1], spacing.y = PixelSpacing[0], et spacing.z = médiane des gaps projetés (mm).

direction = [ row ; col ; normal ] en row-major (3×3).

Les voxels sont stockés en uint16_t (brut en sortie de DicomImage).

rescaleSlope/rescaleIntercept sont conservés pour une segmentation ultérieure (HU).