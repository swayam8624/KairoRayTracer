# KairoRayTracer

`KairoRayTracer` is a standalone CPU renderer in the Kairo workspace. It is the
first visible graphics artifact that forces the foundation stack to work
together:

```text
KairoMath -> KairoGeometry -> KairoSpatial -> rendered image
KairoAssets -> portable mesh artifacts ------^
```

The repo has moved past V1 stabilization into renderer-strengthening work:
HDR output, environment radiance, PPM texture loading, filtered texture
sampling, OBJ UV/normal interpolation, MIS-weighted area-light estimates in the
path tracer, and progressive accumulation are now part of the CPU path. ImGui,
GPU backends, glTF, and denoising remain out of scope for this phase.

## Gallery

| Whitted Cornell | PBR Cornell |
|---|---|
| ![Whitted Cornell](outputs/gallery/cornell_whitted.png) | ![PBR Cornell](outputs/gallery/cornell_pbr.png) |

| Path Tracing | Glass Refraction |
|---|---|
| ![Path tracing showcase](outputs/gallery/path_showcase.png) | ![Glass refraction showcase](outputs/gallery/glass_refraction.png) |

| OBJ Mesh | Area Light Soft Shadows |
|---|---|
| ![OBJ mesh showcase](outputs/gallery/mesh_showcase.png) | ![Area light soft shadow showcase](outputs/gallery/area_light_soft_shadow.png) |

| BVH Heatmap | Acceleration Difference |
|---|---|
| ![BVH heatmap](outputs/gallery/bvh_heatmap.png) | ![Acceleration difference](outputs/gallery/accel_diff.png) |

## Quickstart

Build and test with the module-capable LLVM toolchain:

```bash
cd /Users/swayamsingal/Desktop/Programming/Kairo/KairoRayTracer
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

Render an image:

```bash
./build/KairoRayTracerCLI scenes/cornell.kairo --mode whitted --output outputs/beauty.png
open outputs/beauty.png
```

Render an HDR image without tonemapping/clamping:

```bash
./build/KairoRayTracerCLI scenes/textured_environment_showcase.kairo --mode path --passes 16 --output outputs/textured_environment_path.hdr
open outputs/textured_environment_path.hdr
```

Render with progressive accumulation:

```bash
./build/KairoRayTracerCLI scenes/path_showcase.kairo --mode path --passes 32 --output outputs/path_progressive.png
```

Render at a different resolution:

```bash
./build/KairoRayTracerCLI scenes/cornell.kairo --mode whitted --width 1280 --height 720 --output outputs/cornell_720p.png
```

`--width` and `--height` re-render a new film and update camera aspect ratio.
Resizing the preview window only scales the already-rendered texture.

Headless CLI-only build:

```bash
cmake -S . -B build-headless -G Ninja -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DKAIRO_RAYTRACER_BUILD_PREVIEW=OFF
cmake --build build-headless
```

## Architecture

```text
.kairo scene
  -> SceneParser
  -> materials, lights, primitives, camera, settings
  -> KairoSpatial acceleration structure
  -> Renderer tile loop
  -> integrator/debug mode
  -> Film
  -> PPM/PNG image and optional GLFW preview
```

Core renderer modules do not depend on GLFW or OpenGL. `PreviewWindow.cppm` is
compiled only for `KairoRayTracerPreview` when
`KAIRO_RAYTRACER_BUILD_PREVIEW=ON`.

Wavefront source parsing is owned by KairoAssets. `OBJLoader.cppm` reads the
source, requests a validated `MeshArtifactData`, and only adapts that portable
indexed geometry into ray-tracer triangles. The former duplicate parser has
been removed, so offline and real-time consumers now share index validation,
polygon triangulation, smoothing, generated normals, and source diagnostics.

## Implemented Capabilities

```text
Geometry: spheres, triangles, OBJ meshes with UV and normal interpolation
Acceleration: brute force, SAH BVH, Morton-ordered BVH
Lighting: point lights, rectangular area lights, textured/constant environment
Materials: lambert, mirror, emissive, glass, roughness-metallic PBR, albedo textures
Modes: whitted, pbr, path, normal, depth, shadow_mask, bvh_heatmap,
       albedo, primitive_id, uv, barycentric, accel_diff
Output: PPM, PNG, HDR/RGBE, CSV render stats
Execution: stratified SPP, threaded tiles, deterministic fixed-scene renders,
           progressive accumulation
Preview: GLFW image viewer with save, reload, orbit, zoom, and reset controls
```

## Scene DSL

`.kairo` is a strict line-oriented scene format. `#` starts a comment.
Materials must be declared before primitives that reference them.

```text
resolution width height
samples count
background r g b
texture name relative_or_absolute_ppm_path [nearest|bilinear]
environment constant r g b intensity
environment texture textureName intensity
integrator whitted|pbr|path|normal|depth|shadow_mask|bvh_heatmap|albedo|primitive_id|uv|barycentric|accel_diff
camera px py pz tx ty tz upx upy upz fovDegrees
material name lambert|mirror|emissive r g b [emitR emitG emitB]
material name glass r g b ior
material name pbr r g b roughness metallic
material_texture materialName textureName
light point x y z r g b intensity
light area px py pz ux uy uz vx vy vz r g b intensity samples
sphere x y z radius materialName
triangle ax ay az bx by bz cx cy cz materialName
obj relative_or_absolute_path materialName scale tx ty tz
```

Texture notes:

```text
Only ASCII PPM/P3 textures are loaded in this phase.
Texture values are interpreted as linear RGB.
UVs wrap outside [0, 1], which makes repeated checker/material patterns simple.
OBJ faces may use v, v/vt, v//vn, or v/vt/vn. Polygon faces are fan-triangulated.
```

Example scenes:

```text
cornell.kairo                  Whitted/PBR Cornell-style room
path_showcase.kairo            path tracing smoke and gallery scene
glass_refraction.kairo         glass, refraction, and Fresnel
mesh_showcase.kairo            OBJ loading through triangle primitives
area_light_soft_shadow.kairo   rectangular area light and soft shadows
bvh_stress.kairo               acceleration/debug heatmap scene
textured_environment_showcase.kairo texture loading, OBJ UVs, environment HDR
```

## Debug Modes

```text
normal       surface normal and winding check
depth        camera framing and hit-distance check
shadow_mask  light visibility and ray-bias check
bvh_heatmap  traversal cost visualization
albedo       material assignment without lighting
primitive_id primitive identity and parser ordering
uv           triangle UV/barycentric preservation
barycentric  triangle interpolation coordinates
accel_diff   BVH-vs-brute correctness image; black means match
```

Generate the main debug set:

```bash
./build/KairoRayTracerCLI scenes/cornell.kairo --mode normal --output outputs/normal.png
./build/KairoRayTracerCLI scenes/cornell.kairo --mode depth --output outputs/depth.png
./build/KairoRayTracerCLI scenes/cornell.kairo --mode shadow_mask --output outputs/shadow_mask.png
./build/KairoRayTracerCLI scenes/bvh_stress.kairo --mode bvh_heatmap --output outputs/bvh_heatmap.png
./build/KairoRayTracerCLI scenes/cornell.kairo --mode accel_diff --output outputs/accel_diff.png
```

## Preview Window

```bash
./build/KairoRayTracerPreview scenes/cornell.kairo --mode whitted
```

Controls:

```text
Esc   close
S     save current film
R     reload and re-render the scene file
Arrows orbit camera around the current focus estimate and re-render
Q/E   zoom orbit camera in/out and re-render
Home  reset preview orbit
```

The preview is a lightweight image viewer, not an editor and not ImGui.

## Benchmark Snapshot

Scene: `scenes/bvh_stress.kairo`  
Primitive count: 17  
Resolution: `640x370`  
SPP: `1`  
Threads: `4`  
Machine/date: local Apple Silicon run during stabilization.

| Mode | Primitives | Resolution | SPP | Threads | Time | Rays/s | Nodes/ray | Tests/ray |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Brute force | 17 | 640x370 | 1 | 4 | 185.205 ms | 2.02405e+06 | 0 | 16.8813 |
| BVH SAH | 17 | 640x370 | 1 | 4 | 210.430 ms | 1.78142e+06 | 1.95113 | 2.25284 |
| BVH Morton | 17 | 640x370 | 1 | 4 | 228.282 ms | 1.64211e+06 | 2.24750 | 4.74690 |

Commands:

```bash
./build/KairoRayTracerCLI scenes/bvh_stress.kairo --mode whitted --accel brute --width 640 --height 370 --samples 1 --threads 4 --output outputs/bvh_stress_brute.png --stats outputs/bench_brute.csv
./build/KairoRayTracerCLI scenes/bvh_stress.kairo --mode whitted --accel sah --width 640 --height 370 --samples 1 --threads 4 --output outputs/bvh_stress_sah.png --stats outputs/bench_sah.csv
./build/KairoRayTracerCLI scenes/bvh_stress.kairo --mode whitted --accel morton --width 640 --height 370 --samples 1 --threads 4 --output outputs/bvh_stress_morton.png --stats outputs/bench_morton.csv
```

This benchmark is intentionally reported as a snapshot. With only 17 primitives,
brute force can win wall-clock time because traversal overhead dominates, while
SAH still proves the acceleration structure by cutting primitive tests per ray
from `16.8813` to `2.25284`.

## Stabilization Status

The current test suite covers:

```text
Parser errors with line/column output
Camera center ray
Sphere and triangle hit records
BVH hit equivalence to brute force, including UV/barycentric data
Shadow occlusion
Whitted, PBR, path, OBJ, and area-light render smoke tests
HDR output headers
Texture loading, nearest/bilinear filtering, material texture binding
Environment radiance sampling
OBJ UV and vertex normal preservation
Progressive path accumulation determinism
Path determinism and finite/no-NaN pixels
Single-thread versus multi-thread render equality
SPP 1, 4, and 16 render stability
PNG header/dimension output
CSV stats output
Renderer setting validation
CLI invalid numeric override rejection
```

Validation rejects unsafe direct `RenderSettings` values before rendering:

```text
Width == 0
Height == 0
SamplesPerPixel == 0
TileSize == 0
DepthFar <= DepthNear
Width/Height > 32768
SamplesPerPixel > 4096
TileSize > 4096
MaxDepth > 64
ThreadCount > 1024
```

## Deferred

```text
Denoising experiments
Image texture formats beyond ASCII PPM/P3
Environment importance sampling
Microfacet BSDF sampling for the path tracer
Per-pixel traversal analytics CSV
ImGui debug/editor UI
GPU backend
glTF loading
```
