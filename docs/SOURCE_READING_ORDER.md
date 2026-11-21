# Teaching source map — KairoRayTracer

> A guided source-reading itinerary. It is not a test result or claim that every file is documented yet.

## How to use this guide

Trace a single ray before reading light transport optimization. This is an offline CPU renderer and does not share native GPU swapchain ownership with KairoRenderer.

Read each phase in order. Every link goes to a source file or directory in this repository. For a directory, inspect its public interface and then its implementation files. After each phase, identify the caller, the state it owns, the inputs it validates, and its observable outputs.

## 1. Concepts and build

1. [`README.md`](../README.md)
2. [`CMakeLists.txt`](../CMakeLists.txt)

## 2. Scene and rendering code

1. [`KairoRayTracer.cppm`](../KairoRayTracer.cppm)
2. [`Scene.cppm`](../Scene.cppm)
3. [`RayTracerTypes.cppm`](../RayTracerTypes.cppm)
4. [`Camera.cppm`](../Camera.cppm)
5. [`SceneParser.cppm`](../SceneParser.cppm)
6. [`Primitive.cppm`](../Primitive.cppm)
7. [`Material.cppm`](../Material.cppm)
8. [`WhittedIntegrator.cppm`](../WhittedIntegrator.cppm)
9. [`PathIntegrator.cppm`](../PathIntegrator.cppm)
10. [`Renderer.cppm`](../Renderer.cppm)
11. [`scenes`](../scenes)

## 3. Verification and outputs

1. [`tests`](../tests)
2. [`outputs/gallery`](../outputs/gallery)
3. [`research`](../research)

## End-to-end interaction map

KairoMath supplies vectors/transforms; KairoGeometry gives primitives and ray intersections; KairoSpatial accelerates queries with BVH; KairoAssets supplies portable meshes. The ray tracer evaluates materials, lighting, and recursive/path light transport, then exports CPU-built image files.

## APIs and code-reading questions

For each function, distinguish **contract** (preconditions, postconditions, failure modes), **implementation** (state/algorithms), **collaborators** (imports, calls and consumers), **lifetime/ownership**, and **verification** (test or evidence that checks it). For C++ modules, `export module` names the public compiled interface and `import` expresses module dependencies; an implementation unit's `module ...;` contributes definitions without exporting them.

## Boundaries and honesty

Inspect actual source-tree variants before assuming advanced textures, denoising, GPU rendering or glTF support. Some features are explicitly deferred in README.

## Documentation rules

- Teaching comments describe *why*, invariants, edge cases, API ownership and cross-file effects; avoid narrating obvious assignments.
- Preserve behavior during documentation-only edits; keep correctness changes in separate reviewed commits.
- Link function names and tests where possible; mark claims as source-present, CI-observed, host-verified or research-evidenced.
- License grants apply only to material for which the repository owner holds rights; third-party source, models, media and datasets keep their own notices.


## Complete source-file inventory

After the curated teaching path, follow this deterministic inventory of first-party source, scripts, shaders, and tests. Grouping is a suggested learning order, not an indication that every function is already annotated. Vendored dependencies, ROMs, binaries, data, media, and generated evidence are excluded.

### Entry points and build (1)

1. [`CMakeLists.txt`](../CMakeLists.txt)

### Interfaces and modules (27)

1. [`Camera.cppm`](../Camera.cppm)
2. [`Color.cppm`](../Color.cppm)
3. [`DepthIntegrator.cppm`](../DepthIntegrator.cppm)
4. [`Environment.cppm`](../Environment.cppm)
5. [`Film.cppm`](../Film.cppm)
6. [`ImageIO.cppm`](../ImageIO.cppm)
7. [`Integrator.cppm`](../Integrator.cppm)
8. [`KairoRayTracer.cppm`](../KairoRayTracer.cppm)
9. [`Light.cppm`](../Light.cppm)
10. [`Material.cppm`](../Material.cppm)
11. [`Mesh.cppm`](../Mesh.cppm)
12. [`NormalIntegrator.cppm`](../NormalIntegrator.cppm)
13. [`OBJLoader.cppm`](../OBJLoader.cppm)
14. [`OfflineRenderJob.cppm`](../OfflineRenderJob.cppm)
15. [`PathIntegrator.cppm`](../PathIntegrator.cppm)
16. [`PBRIntegrator.cppm`](../PBRIntegrator.cppm)
17. [`PreviewWindow.cppm`](../PreviewWindow.cppm)
18. [`Primitive.cppm`](../Primitive.cppm)
19. [`ProgressiveRenderer.cppm`](../ProgressiveRenderer.cppm)
20. [`RayTracerTypes.cppm`](../RayTracerTypes.cppm)
21. [`Renderer.cppm`](../Renderer.cppm)
22. [`Sampler.cppm`](../Sampler.cppm)
23. [`Scene.cppm`](../Scene.cppm)
24. [`SceneParser.cppm`](../SceneParser.cppm)
25. [`Shading.cppm`](../Shading.cppm)
26. [`Texture.cppm`](../Texture.cppm)
27. [`WhittedIntegrator.cppm`](../WhittedIntegrator.cppm)

### Implementation and other source (2)

1. [`examples/cli.cpp`](../examples/cli.cpp)
2. [`examples/preview.cpp`](../examples/preview.cpp)

### Verification and tests (3)

1. [`tests/OfflineRenderJobTests.cpp`](../tests/OfflineRenderJobTests.cpp)
2. [`tests/RayTracerTests.cpp`](../tests/RayTracerTests.cpp)
3. [`tests/SharedLightSemanticsTests.cpp`](../tests/SharedLightSemanticsTests.cpp)

Snapshot totals: 33 first-party source/build/test files, 70 tracked files overall. Re-audit when files are renamed or added.
