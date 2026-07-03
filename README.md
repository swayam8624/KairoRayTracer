# KairoRayTracer

`KairoRayTracer` is a standalone CPU ray tracer under the Kairo workspace. It is
the first visible graphics artifact that forces the foundation stack to work
together:

```text
KairoMath -> KairoGeometry -> KairoSpatial -> rendered image
```

V1 intentionally avoids ImGui, Vulkan, Metal, JSON, PNG, OBJ, and path tracing.
It loads `.kairo` scene files, renders on the CPU, saves PPM images, and can show
the completed render in a small GLFW/OpenGL preview window.

## Build

```bash
cd /Users/swayamsingal/Desktop/Programming/Kairo/KairoRayTracer
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

If the preview target cannot find GLFW:

```bash
brew install glfw
```

## Render To Image

```bash
./build/KairoRayTracerCLI scenes/cornell.kairo --mode whitted --output outputs/beauty.ppm
open outputs/beauty.ppm
```

Do not run image paths directly:

```bash
outputs/beauty.ppm      # wrong: the shell tries to execute the image
open outputs/beauty.ppm # correct: opens the image in Preview
```

Lines beginning with `#` in README examples are comments, not commands to paste
after the command prompt.

Debug modes:

```bash
./build/KairoRayTracerCLI scenes/cornell.kairo --mode normal --output outputs/normal.ppm
./build/KairoRayTracerCLI scenes/cornell.kairo --mode depth --output outputs/depth.ppm
./build/KairoRayTracerCLI scenes/cornell.kairo --mode shadow_mask --output outputs/shadow_mask.ppm
./build/KairoRayTracerCLI scenes/cornell.kairo --mode bvh_heatmap --output outputs/bvh_heatmap.ppm
```

## Preview Window

```bash
./build/KairoRayTracerPreview scenes/cornell.kairo --mode whitted
```

Controls:

```text
Esc  close
S    save current film to outputs/*.ppm
R    reload and re-render the same scene file
```

The preview is only an image viewer for V1. It is not an editor and does not use
ImGui.

## Scene Format

`.kairo` is line-oriented. `#` starts a comment.

```text
resolution width height
samples count
background r g b
integrator whitted|normal|depth|shadow_mask|bvh_heatmap
camera px py pz tx ty tz upx upy upz fovDegrees
material name lambert|mirror|emissive r g b [emitR emitG emitB]
light point x y z r g b intensity
sphere x y z radius materialName
triangle ax ay az bx by bz cx cy cz materialName
```

Example scenes:

```text
scenes/cornell.kairo
scenes/reflective_spheres.kairo
scenes/triangle_floor.kairo
```

## V1 Features

```text
Sphere and triangle primitives
Variant primitive storage
KairoSpatial BVH acceleration
Brute-force intersection path for tests
Normal, depth, Whitted, shadow-mask, and BVH-heatmap render modes
Point lights, hard shadows, emissive materials, and perfect mirror reflection
PPM output
Minimal GLFW preview
```

## Deferred

```text
PNG output
OBJ/glTF loading
Progressive rendering
Camera orbit controls
PBR and path tracing
ImGui debug/editor UI
GPU backend
```
