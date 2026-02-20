cmake_minimum_required(VERSION 3.23)
project(Dx11MeshViewer LANGUAGES CXX)


# Options générales
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# VCPKG: détection automatique si le toolchain n'est pas déjà défini
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        message(STATUS "Using VCPKG from ENV: $ENV{VCPKG_ROOT}")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain")
    elseif(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        message(STATUS "Using VCPKG from C:/vcpkg")
        set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake")
        message(STATUS "Using local VCPKG: external/vcpkg")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain")
    else()
        message(WARNING "VCPKG toolchain not found. Run scripts/setup_vcpkg.ps1 or set VCPKG_ROOT.")
    endif()
endif()


# Sources
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    src/*.cpp 
    src/*.h
)



# AVANT (console par défaut)
# add_executable(${PROJECT_NAME} ${SRC_FILES} shaders/mesh.hlsl)

# APRÈS (GUI -> wWinMain)
add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES}  
"external/tinyobjloader/tiny_obj_loader.cpp" 
"src/core/Volume.h" "src/core/Segmentation.h" 
"src/core/Segmentation.cpp" "src/core/Types.h" 
"src/core/VolumeCleaning.h" "src/core/IsoSurfaceMC.h" 
"src/core/IsoSurfaceMC.cpp" "src/core/Curvature.h" 
"src/core/Curvature.cpp" "src/core/Decimation.h" 
"src/core/Decimation.cpp" "src/io/ObjExporter.h" 
"src/io/ObjExporter.cpp" "src/utils/MathUtils.h")


# Définir l'icône ou res (optionnel)


# Windows et D3D11
if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 dxgi d3dcompiler dxguid d2d1 dwrite)
endif()


# vcpkg packages
find_package(fmt CONFIG REQUIRED)
find_package(directxmath CONFIG REQUIRED)
find_package(DCMTK CONFIG REQUIRED)


target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

#verifier les noms des targets DCMTK avec la version installée
target_link_libraries(${PROJECT_NAME} PRIVATE Microsoft::DirectXMath DCMTK::dcmdata DCMTK::ofstd DCMTK::dcmimgle DCMTK::dcmimage)


# Warnings & secure CRT
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /Zc:__cplusplus)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()


# Folders dans VS
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${SRC_FILES})


# Copie des shaders à côté de l'exe pour le chargement runtime
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.hlsl
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/mesh.hlsl
)