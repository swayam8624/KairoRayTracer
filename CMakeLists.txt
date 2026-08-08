cmake_minimum_required(VERSION 3.28)
project(KairoRayTracer LANGUAGES CXX)

# KairoRayTracer is intentionally its own repo/project, not a Foundation
# subfolder. It consumes the foundation stack the way a real graphics artifact
# would: Math -> Geometry -> Spatial -> renderer.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KAIRO_RAYTRACER_BUILD_TESTS "Build KairoRayTracer tests" ${PROJECT_IS_TOP_LEVEL})
option(KAIRO_RAYTRACER_BUILD_PREVIEW "Build GLFW preview executable" ON)

find_package(Threads REQUIRED)

# Standalone fallbacks are immutable merged revisions validated by the umbrella
# integration build. Local sibling checkouts remain preferred during workspace
# development.
set(KAIRO_RAYTRACER_SPATIAL_REVISION 860aa8024e4cd14146165f91f31a70afe1bd0eee)
set(KAIRO_RAYTRACER_ASSETS_REVISION 1db4c25ecc4aeff7b6fc491c5197fdecf7e790bb)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
elseif(MSVC)
    add_compile_options(/W4)
endif()

if(NOT TARGET KairoAssets)
    set(KAIRO_ASSETS_SOURCE_DIR "" CACHE PATH "Path to a local KairoAssets checkout")
    if(KAIRO_ASSETS_SOURCE_DIR AND EXISTS "${KAIRO_ASSETS_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${KAIRO_ASSETS_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/KairoAssets")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../KairoAssets/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../KairoAssets" "${CMAKE_CURRENT_BINARY_DIR}/_deps/KairoAssets")
    else()
        include(FetchContent)
        FetchContent_Declare(KairoAssets
            GIT_REPOSITORY https://github.com/swayam8624/KairoAssets.git
            GIT_TAG ${KAIRO_RAYTRACER_ASSETS_REVISION})
        FetchContent_MakeAvailable(KairoAssets)
    endif()
endif()

if(NOT TARGET KairoSpatial)
    set(KAIRO_SPATIAL_SOURCE_DIR "" CACHE PATH "Path to a local KairoSpatial checkout")
    if(KAIRO_SPATIAL_SOURCE_DIR AND EXISTS "${KAIRO_SPATIAL_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${KAIRO_SPATIAL_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/KairoSpatial")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Foundation/Spatial/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../Foundation/Spatial" "${CMAKE_CURRENT_BINARY_DIR}/_deps/KairoSpatial")
    else()
        include(FetchContent)
        FetchContent_Declare(KairoSpatial
            GIT_REPOSITORY https://github.com/swayam8624/KairoSpatial.git
            GIT_TAG ${KAIRO_RAYTRACER_SPATIAL_REVISION})
        FetchContent_MakeAvailable(KairoSpatial)
    endif()
endif()

add_library(KairoRayTracer)
target_sources(KairoRayTracer
    PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            Color.cppm
            RayTracerTypes.cppm
            Film.cppm
            ImageIO.cppm
            Texture.cppm
            Environment.cppm
            Camera.cppm
            Material.cppm
            Light.cppm
            Primitive.cppm
            Mesh.cppm
            OBJLoader.cppm
            Scene.cppm
            Shading.cppm
            SceneParser.cppm
            Sampler.cppm
            Integrator.cppm
            NormalIntegrator.cppm
            DepthIntegrator.cppm
            WhittedIntegrator.cppm
            PBRIntegrator.cppm
            PathIntegrator.cppm
            Renderer.cppm
            ProgressiveRenderer.cppm
            OfflineRenderJob.cppm
            KairoRayTracer.cppm
)

target_link_libraries(KairoRayTracer PUBLIC KairoSpatial KairoAssets Threads::Threads)

add_executable(KairoRayTracerCLI examples/cli.cpp)
target_link_libraries(KairoRayTracerCLI PRIVATE KairoRayTracer)

if(KAIRO_RAYTRACER_BUILD_PREVIEW)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    add_executable(KairoRayTracerPreview examples/preview.cpp)
    target_sources(KairoRayTracerPreview
        PRIVATE
            FILE_SET CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES PreviewWindow.cppm)
    target_link_libraries(KairoRayTracerPreview PRIVATE KairoRayTracer glfw OpenGL::GL)
endif()

if(KAIRO_RAYTRACER_BUILD_TESTS)
    find_package(Git REQUIRED)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
        GIT_SUBMODULES ""
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)

    add_executable(KairoRayTracerTests
        tests/RayTracerTests.cpp
        tests/OfflineRenderJobTests.cpp
        tests/SharedLightSemanticsTests.cpp)
    target_link_libraries(KairoRayTracerTests PRIVATE KairoRayTracer Catch2::Catch2WithMain)

    enable_testing()
    add_test(NAME KairoRayTracerTests COMMAND KairoRayTracerTests)

    add_test(NAME KairoRayTracerCLIRejectsZeroWidth COMMAND KairoRayTracerCLI
        ${CMAKE_CURRENT_SOURCE_DIR}/scenes/cornell.kairo --width 0)
    set_tests_properties(KairoRayTracerCLIRejectsZeroWidth PROPERTIES WILL_FAIL TRUE)
    add_test(NAME KairoRayTracerCLIRejectsNegativeSamples COMMAND KairoRayTracerCLI
        ${CMAKE_CURRENT_SOURCE_DIR}/scenes/cornell.kairo --samples -1)
    set_tests_properties(KairoRayTracerCLIRejectsNegativeSamples PROPERTIES WILL_FAIL TRUE)
    add_test(NAME KairoRayTracerCLIRejectsNonNumericThreads COMMAND KairoRayTracerCLI
        ${CMAKE_CURRENT_SOURCE_DIR}/scenes/cornell.kairo --threads nope)
    set_tests_properties(KairoRayTracerCLIRejectsNonNumericThreads PROPERTIES WILL_FAIL TRUE)
endif()
