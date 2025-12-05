#include "io/DICOMLoader.h"   // ton header qui expose IDicomVolumeLoader, MakeDcmtkVolumeLoader, VolumeData
#include "scene/MeshData.h"
#include "app/App.h"
#include <vector>
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include "core/Segmentation.h" // SegmentationRules, SegmentVolumeHU, LabeledVolume
#include "core/VolumeCleaning.h"
#include "core/IsoSurfaceMC.h"
#include "core/Decimation.h"
#include "io/OBJExporter.h"


static void PrintVolumeInfo(const VolumeData& v)
{
    std::cout << "=== Volume Info ===\n";
    std::cout << "Dimensions : " << v.width << " x " << v.height << " x " << v.depth << "\n";
    std::cout << "Spacing    : (" << v.spacing.x << ", " << v.spacing.y << ", " << v.spacing.z << ") mm\n";
    std::cout << "Origin     : (" << v.origin.x << ", " << v.origin.y << ", " << v.origin.z << ") mm\n";
    std::cout << "Direction  :\n";
    std::cout << "  [ " << v.direction[0] << " " << v.direction[1] << " " << v.direction[2] << " ]\n";
    std::cout << "  [ " << v.direction[3] << " " << v.direction[4] << " " << v.direction[5] << " ]\n";
    std::cout << "  [ " << v.direction[6] << " " << v.direction[7] << " " << v.direction[8] << " ]\n";
    const size_t expected = static_cast<size_t>(v.width) * v.height * v.depth;
    std::cout << "Voxels count      = " << v.voxels.size()
        << " (attendu = " << expected << ")\n";
    if (v.voxels.size() != expected) {
        std::cout << "[WARN] Taille du buffer inattendue.\n";
    }
}

static int TestDicomLoader(const std::string& folder)
{
    try {
        DICOMLoader loader;
        VolumeData vol = loader.loadFromDirectory(folder);
        PrintVolumeInfo(vol);
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Erreur: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}

// Petit utilitaire pour creer un cube 1x1x1 centr� (exemple)
static MeshData MakeCube()
{
    MeshData m;
    // 6 faces * 4 sommets = 24 positions/normales
    const Vec3f P[] = {
        // +Z (front)
        {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
        // -Z (back)
        { 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f},
        // +X (right)
        { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f},
        // -X (left)
        {-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f},
        // +Y (top)
        {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
        // -Y (bottom)
        {-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f},
    };
    const Vec3f N[] = {
        // +Z
        {0,0,1},{0,0,1},{0,0,1},{0,0,1},
        // -Z
        {0,0,-1},{0,0,-1},{0,0,-1},{0,0,-1},
        // +X
        {1,0,0},{1,0,0},{1,0,0},{1,0,0},
        // -X
        {-1,0,0},{-1,0,0},{-1,0,0},{-1,0,0},
        // +Y
        {0,1,0},{0,1,0},{0,1,0},{0,1,0},
        // -Y
        {0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},
    };
    const uint32_t I[] = {
        0,1,2, 0,2,3,      4,5,6, 4,6,7,
        8,9,10, 8,10,11,  12,13,14, 12,14,15,
        16,17,18, 16,18,19, 20,21,22, 20,22,23
    };
    m.positions.assign(std::begin(P), std::end(P));
    m.normals.assign(std::begin(N), std::end(N));
    m.indices.assign(std::begin(I), std::end(I));
    return m;
}


int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	App app;
    std::vector<MeshData> meshes;

	//bindMeshes(&meshes, const& directory);
	meshes.push_back(MakeCube());
	meshes.push_back(MakeCube());

	std::string folder = "C:\\projects\\dcmfiles";
	//TestDicomLoader(folder);

    try
    {
        DICOMLoader loader;
        VolumeData volume = loader.loadFromDirectory(folder);

        // 2) Cr�er le segmented volume (HU -> labels)
        SegmentationRules rules;               // seuils par d�faut (os/soft/fat)
        LabeledVolume segmentedVolume = SegmentVolumeHU(volume, rules);

        // (optionnel) petit r�sum�
        //std::cout << "Volume : " << volume.width << " x " << volume.height << " x " << volume.depth << "\n";
        //std::cout << "Voxels : " << volume.voxels.size() << "\n";
        //std::cout << "Labels : " << segmentedVolume.labels.size() << "\n";
        //return EXIT_SUCCESS;

        //Test MC
        // Nettoyage
        //core::MorphologyParams mp;
        //mp.radius = 1;
        //mp.iterations = 1;
        //mp.do_open_close = true;
        //core::CleanLabeledVolume(segmentedVolume, mp); // en place

        // Extraction des surfaces
        //auto meshesMC = core::extractMeshes(segmentedVolume);
        // ou bien par classe :
        MeshData bone = core::marchingcubes(segmentedVolume, TissueLabel::Bone);

        //for (auto m : meshesMC)
        //{
        //   meshes.push_back(bone);
        //}

        // Exemple d'usage Decimate
        DecimationSettings ds;
        //ds.factor = 0.0f;           // 0.0 = pas de d�cimation, 1.0 = agressif
        //ds.protectTopPercent = 0.10f;
        //ds.cellScale = 1.0f;

        MeshData skinDecim = DecimateByCurvature(bone, ds);


        // Visualiser avant / apr�s (couleurs diff�rentes)
        Color3f cBefore{ 0.85f, 0.85f, 1.0f };
        Color3f cAfter{ 0.90f, 0.60f, 0.60f };
        //app.addMesh(bone, &cBefore);
        app.addMesh(bone, &cAfter);

        //ExportOBJ("C:\\projects\\DX11MeshViewer\\meshes\\skin_original.obj", skinDecim);


    }

    catch (const std::exception & e) {
        std::cerr << "Erreur: " << e.what() << "\n";
        return EXIT_FAILURE;
    }



	// Lancer l'app, mais on veut d'abord initialiser les ressources GPU :
	// On ajoute les meshes apr�s que le device existe, i.e. � l'int�rieur de App::run().
	// Pour concilier simplicit�, on cr�e un App, on le lance, et on injecterait
	// normalement les donn�es via une API avant la boucle. Ici, on simule cela
	// en modifiant App pour qu'il expose device() � d�j� fait � puis on pr�pare la sc�ne.


	// Au plus simple : on d�marre l'app puis on y ajoute les meshes via un callback
	// � Pour cette d�mo, on les ajoutera dans App::run() avant la boucle si n�cessaire.


	// Variante : on ajoute ici en deux temps en exploitant l'API publique:
	// (On va d�marrer l'app et, pour l'exemple, on pousse les meshes tout de suite apr�s cr�ation du device.)


	// Simplification : on appelle directement run(), et si vous avez d�j� vos MeshData,
	// adaptez App pour les ajouter juste apres initialize().

    Color3f c0{ 1.0f, 0.9f, 0.95f };

	for (auto& m : meshes) {
        app.addMesh(m, &c0);
        c0.r *= 0.9f; c0.g *= 0.9f; c0.b *= 1.1f; // juste pour varier les couleurs
	}


	app.run();
	return 0;
}


/*
NB: pour un flux reel, 
exposez une fonction 
App::loadMeshes(const std::vector<std::pair<MeshData, std::optional<Color3f>>>&) 
et appelez-la apres renderer.initialize(...).
*/