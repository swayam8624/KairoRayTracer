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

The core library is CLI/headless friendly. GLFW and OpenGL are required only
when `KAIRO_RAYTRACER_BUILD_PREVIEW=ON`.

## How To Read This Project

Read the code in this order if you want to master the concepts:

```text
Color.cppm          linear color, gamma, output bytes
Film.cppm           image memory layout
Camera.cppm         pinhole camera and primary rays
Primitive.cppm      sphere/triangle bounds and intersections
Scene.cppm          render objects mapped into KairoSpatial BVH
SceneParser.cppm    .kairo text scene format
NormalIntegrator    normal visualization
DepthIntegrator     depth visualization
WhittedIntegrator   lighting, shadows, recursive mirror rays
Renderer.cppm       pixel loop and integrator dispatch
PreviewWindow.cppm  CPU film displayed in a GLFW/OpenGL window
```

The most important mental model:

```text
scene file
  -> parser
  -> Scene materials/lights/primitives
  -> primitive bounds
  -> KairoSpatial BVH
  -> camera ray per pixel
  -> exact primitive intersection
  -> integrator shading
  -> Film
  -> PPM file and optional preview window
```

## Build

```bash
cd /Users/swayamsingal/Desktop/Programming/Kairo/KairoRayTracer
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

Headless CLI-only build:

```bash
cmake -S . -B build-headless -G Ninja -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DKAIRO_RAYTRACER_BUILD_PREVIEW=OFF
cmake --build build-headless
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
material name glass r g b ior
material name pbr r g b roughness metallic
light point x y z r g b intensity
sphere x y z radius materialName
triangle ax ay az bx by bz cx cy cz materialName
obj relative_or_absolute_path materialName scale tx ty tz
```

V1 requires materials to be declared before primitives that reference them.

Example scenes:

```text
scenes/cornell.kairo
scenes/reflective_spheres.kairo
scenes/triangle_floor.kairo
scenes/shadow_lab.kairo
scenes/mirror_hall.kairo
scenes/bvh_stress.kairo
scenes/normal_depth_lab.kairo
scenes/emissive_showcase.kairo
scenes/parser_reference.kairo
scenes/glass_refraction.kairo
scenes/mesh_showcase.kairo
scenes/pbr_showcase.kairo
```

Scene purpose:

```text
cornell.kairo            room, colored walls, mirror object, good default beauty test
reflective_spheres.kairo open floor and multiple lights for readable reflections
triangle_floor.kairo     simple normal/debug scene
shadow_lab.kairo         hard-shadow blocker test
mirror_hall.kairo        recursive mirror stress test
bvh_stress.kairo         many primitives for bvh_heatmap traversal visualization
normal_depth_lab.kairo   clean shapes for normal/depth render modes
emissive_showcase.kairo  visible emissive geometry plus colored lights
parser_reference.kairo   compact reference for every V1 scene command
glass_refraction.kairo   glass material and Schlick Fresnel/refraction test
mesh_showcase.kairo      OBJ mesh loading converted into triangle primitives
pbr_showcase.kairo       Cook-Torrance/GGX direct lighting demo
```

Render every main debug output for the default scene:

```bash
./build/KairoRayTracerCLI scenes/cornell.kairo --mode whitted --output outputs/beauty.ppm
./build/KairoRayTracerCLI scenes/cornell.kairo --mode normal --output outputs/normal.ppm
./build/KairoRayTracerCLI scenes/cornell.kairo --mode depth --output outputs/depth.ppm
./build/KairoRayTracerCLI scenes/cornell.kairo --mode shadow_mask --output outputs/shadow_mask.ppm
./build/KairoRayTracerCLI scenes/cornell.kairo --mode bvh_heatmap --output outputs/bvh_heatmap.ppm
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

## Concept Notes

Ray tracing:

```text
For each pixel, the camera creates a ray. The scene finds the nearest surface
hit. The integrator decides what color that hit should produce.
```

BVH acceleration:

```text
Brute force tests every primitive for every ray. BVH traversal first rejects
large empty regions using bounding boxes, then tests only likely primitives.
```

Whitted shading:

```text
Whitted rendering is direct lighting plus recursive perfect reflections. It is
not global illumination; diffuse surfaces do not bounce indirect light yet.
```

Shadow rays:

```text
After a primary ray hits a surface, a secondary ray is sent toward each light.
If anything blocks that segment, that light contributes nothing at the point.
```

Debug render modes:

```text
normal       checks surface normals and winding
depth        checks nearest-hit distance and camera framing
shadow_mask  checks light visibility and self-intersection bias
bvh_heatmap  checks acceleration traversal cost
albedo       checks material assignment without lighting
primitive_id checks primitive identity and parser ordering
uv           checks triangle UV/barycentric preservation
barycentric  checks triangle interpolation coordinates
accel_diff   checks BVH output against brute force
whitted      combines geometry, BVH, materials, lights, shadows, and reflection
```
