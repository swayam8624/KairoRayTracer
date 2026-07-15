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

# Standalone source fallback aligned with the spatial revision exercised by the
# workspace superbuild. Local sibling and explicit source paths remain first.
set(KAIRO_RAYTRACER_SPATIAL_REVISION bf238e02cc0b78e07c37a51c5343b726c4b29347)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
elseif(MSVC)
    add_compile_options(/W4)
endif()

if(NOT TARGET KairoSpatial)
    set(KAIRO_SPATIAL_SOURCE_DIR "" CACHE PATH "Path to a local KairoSpatial checkout")

    # Dependency resolution order:
    # 1. Explicit local override for development.
    # 2. Sibling workspace checkout used on this machine.
    # 3. GitHub fallback for standalone clone/build.
    if(KAIRO_SPATIAL_SOURCE_DIR AND EXISTS "${KAIRO_SPATIAL_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory(
            "${KAIRO_SPATIAL_SOURCE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/_deps/KairoSpatial"
        )
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Foundation/Spatial/CMakeLists.txt")
        add_subdirectory(
            "${CMAKE_CURRENT_SOURCE_DIR}/../Foundation/Spatial"
            "${CMAKE_CURRENT_BINARY_DIR}/_deps/KairoSpatial"
        )
    else()
        include(FetchContent)
        FetchContent_Declare(
            KairoSpatial
            GIT_REPOSITORY https://github.com/swayam8624/KairoSpatial.git
            GIT_TAG ${KAIRO_RAYTRACER_SPATIAL_REVISION}
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(KairoSpatial)
    endif()
endif()

add_library(KairoRayTracer)

# C++23 module file set. Keep this list ordered roughly from leaf concepts to
# high-level orchestration so module dependency errors are easier to read.
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
            KairoRayTracer.cppm
)

target_link_libraries(KairoRayTracer PUBLIC KairoSpatial)

add_executable(KairoRayTracerCLI examples/cli.cpp)
target_link_libraries(KairoRayTracerCLI PRIVATE KairoRayTracer)

if(KAIRO_RAYTRACER_BUILD_PREVIEW)
    find_package(OpenGL REQUIRED)
    # glfw3's exported target carries both include directories and platform
    # link requirements. It works with Homebrew and vcpkg, unlike a
    # hard-coded macOS path search.
    find_package(glfw3 CONFIG REQUIRED)

    add_executable(KairoRayTracerPreview examples/preview.cpp)
    target_sources(KairoRayTracerPreview
        PRIVATE
            FILE_SET CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES
                PreviewWindow.cppm
    )
    target_link_libraries(KairoRayTracerPreview PRIVATE KairoRayTracer glfw OpenGL::GL)
endif()

if(KAIRO_RAYTRACER_BUILD_TESTS)
    find_package(Git REQUIRED)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
        # Catch2 has no required submodules. Avoid an unnecessary recursive
        # checkout in FetchContent's isolated population project.
        GIT_SUBMODULES ""
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(KairoRayTracerTests tests/RayTracerTests.cpp)
    target_link_libraries(KairoRayTracerTests PRIVATE KairoRayTracer Catch2::Catch2WithMain)

    enable_testing()
    add_test(NAME KairoRayTracerTests COMMAND KairoRayTracerTests)

    add_test(
        NAME KairoRayTracerCLIRejectsZeroWidth
        COMMAND KairoRayTracerCLI
            ${CMAKE_CURRENT_SOURCE_DIR}/scenes/cornell.kairo
            --width 0
    )
    set_tests_properties(KairoRayTracerCLIRejectsZeroWidth PROPERTIES WILL_FAIL TRUE)

    add_test(
        NAME KairoRayTracerCLIRejectsNegativeSamples
        COMMAND KairoRayTracerCLI
            ${CMAKE_CURRENT_SOURCE_DIR}/scenes/cornell.kairo
            --samples -1
    )
    set_tests_properties(KairoRayTracerCLIRejectsNegativeSamples PROPERTIES WILL_FAIL TRUE)

    add_test(
        NAME KairoRayTracerCLIRejectsNonNumericThreads
        COMMAND KairoRayTracerCLI
            ${CMAKE_CURRENT_SOURCE_DIR}/scenes/cornell.kairo
            --threads nope
    )
    set_tests_properties(KairoRayTracerCLIRejectsNonNumericThreads PROPERTIES WILL_FAIL TRUE)
endif()
