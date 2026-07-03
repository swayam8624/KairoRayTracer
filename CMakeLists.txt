cmake_minimum_required(VERSION 3.28)
project(KairoRayTracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KAIRO_RAYTRACER_BUILD_TESTS "Build KairoRayTracer tests" ${PROJECT_IS_TOP_LEVEL})
option(KAIRO_RAYTRACER_BUILD_PREVIEW "Build GLFW preview executable" ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
elseif(MSVC)
    add_compile_options(/W4)
endif()

if(NOT TARGET KairoSpatial)
    set(KAIRO_SPATIAL_SOURCE_DIR "" CACHE PATH "Path to a local KairoSpatial checkout")

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
            GIT_TAG main
        )
        FetchContent_MakeAvailable(KairoSpatial)
    endif()
endif()

find_package(OpenGL REQUIRED)
find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h PATHS /opt/homebrew/include /usr/local/include)
find_library(GLFW_LIBRARY glfw PATHS /opt/homebrew/lib /usr/local/lib)

if(NOT GLFW_INCLUDE_DIR OR NOT GLFW_LIBRARY)
    message(FATAL_ERROR "GLFW was not found. Install it with: brew install glfw")
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
            Camera.cppm
            Material.cppm
            Light.cppm
            Primitive.cppm
            Scene.cppm
            SceneParser.cppm
            Integrator.cppm
            NormalIntegrator.cppm
            DepthIntegrator.cppm
            WhittedIntegrator.cppm
            Renderer.cppm
            PreviewWindow.cppm
            KairoRayTracer.cppm
)

target_link_libraries(KairoRayTracer PUBLIC KairoSpatial)
target_include_directories(KairoRayTracer PUBLIC ${GLFW_INCLUDE_DIR})
target_link_libraries(KairoRayTracer PUBLIC ${GLFW_LIBRARY} OpenGL::GL)

add_executable(KairoRayTracerCLI examples/cli.cpp)
target_link_libraries(KairoRayTracerCLI PRIVATE KairoRayTracer)

if(KAIRO_RAYTRACER_BUILD_PREVIEW)
    add_executable(KairoRayTracerPreview examples/preview.cpp)
    target_link_libraries(KairoRayTracerPreview PRIVATE KairoRayTracer)
endif()

if(KAIRO_RAYTRACER_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(KairoRayTracerTests tests/RayTracerTests.cpp)
    target_link_libraries(KairoRayTracerTests PRIVATE KairoRayTracer Catch2::Catch2WithMain)

    enable_testing()
    add_test(NAME KairoRayTracerTests COMMAND KairoRayTracerTests)
endif()
