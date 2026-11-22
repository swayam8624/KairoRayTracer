# Teaching source map — KairoRayTracer

> A guided source-reading itinerary. It is not a test result or claim that every file is documented yet.

## How to use this guide

Trace a single ray before reading light transport optimization. This is an offline CPU renderer and does not share native GPU swapchain ownership with KairoRenderer.

Read each phase in order. Every link goes to a source file or directory in this repository. For a directory, inspect its public interface and then its implementation files. After each phase, identify the caller, the state it owns, the inputs it validates, and its observable outputs.

**Work through one actual pixel:** [Camera sample → exact BVH hit → shadow/light PDFs → diffuse bounce → CPU film](PATH_TRACING_LIGHT_SAMPLING_AND_BVH_LAB.md). The guide records the current area-light sampler's four-position repetition and the limits of its MIS claim.

**Work one image in detail:** [Pixel stratification, per-thread tile ownership, exact BVH hits, cosine PDFs and area-light MIS caveats](PIXEL_SEED_TO_FILM_THREADING_LAB.md). It gives numerical and concurrency exercises without treating a rendered screenshot as proof of unbiasedness.

## Stage A — Pixel data, rays and scene ownership

Read these before changing the renderer's threading or physics-looking
intersection helpers. The offline ray tracer produces CPU images rather
than controlling the GPU swapchain in KairoRenderer.

1. [`README.md`](../README.md) and [`CMakeLists.txt`](../CMakeLists.txt): supported scope, targets and compiler.
2. [`KairoRayTracer.cppm`](../KairoRayTracer.cppm): what `export import` makes public.
3. [`RayTracerTypes.cppm`](../RayTracerTypes.cppm): `RenderSettings`, modes, result types and statistics.
4. [`Color.cppm`](../Color.cppm): radiance/color arithmetic and numerical assumptions.
5. [`Film.cppm`](../Film.cppm): CPU pixel storage and ownership.
6. [`ImageIO.cppm`](../ImageIO.cppm): persistence/output formats and transfer conventions.
7. [`Camera.cppm`](../Camera.cppm): normalized film coordinates to `Rayf`.
8. [`Sampler.cppm`](../Sampler.cppm): pixel jitter, deterministic seed and sample indexing.

**Exercise:** trace one pixel from `(x,y)` and sample number through
`StratifiedPixelSample`, `Camera::GenerateRay` and the eventual `Film`
write. Explain why changing ThreadCount should not alter which pixel
sample coordinates are generated.

## Stage B — Render primitives and scene acceleration

9. [`Material.cppm`](../Material.cppm): material indices and available types.
10. [`Texture.cppm`](../Texture.cppm): UVs, image sampling and filtering.
11. [`Light.cppm`](../Light.cppm): light records and physical parameters.
12. [`Environment.cppm`](../Environment.cppm): radiance when a ray misses.
13. [`Primitive.cppm`](../Primitive.cppm): sphere/triangle variants and exact intersection.
14. [`Mesh.cppm`](../Mesh.cppm) and [`OBJLoader.cppm`](../OBJLoader.cppm): mesh inputs to render primitives.
15. [`SceneParser.cppm`](../SceneParser.cppm): scene ingestion and validation.
16. [`Scene.cppm`](../Scene.cppm): authoritative primitive arrays, BVH construction, closest hit and occlusion.

**Exercise:** compare `Scene::BruteForceIntersect` with
`Scene::Intersect` after `BuildAcceleration`. Follow the BVH callback
into [KairoSpatial's traversal](https://github.com/swayam8624/KairoSpatial/blob/main/BVHTraversal.cppm).
The BVH narrows candidate triangles; a primitive-specific callback
computes the real surface intersection. `Scene::Intersect` recomputes
the nearest surface hit to recover material/UV metadata absent from the
spatial-only hit record. Rebuild acceleration after modifying primitives.

## Stage C — Integrators, probability and light transport

17. [`Integrator.cppm`](../Integrator.cppm): shared rendering dispatch/contract ideas.
18. [`Shading.cppm`](../Shading.cppm): light/material evaluation shared across modes.
19. [`NormalIntegrator.cppm`](../NormalIntegrator.cppm): debug normals to color.
20. [`DepthIntegrator.cppm`](../DepthIntegrator.cppm): depth image semantics.
21. [`WhittedIntegrator.cppm`](../WhittedIntegrator.cppm): recursive specular/refraction approach.
22. [`PBRIntegrator.cppm`](../PBRIntegrator.cppm): direct physically based shading path.
23. [`PathIntegrator.cppm`](../PathIntegrator.cppm): stochastic diffuse bounces, environment/emission, roulette and direct-light queries.

**Exercise:** compare the same scene in normal, depth, Whitted, PBR and
path modes. Check the direct-light area PDF against the BSDF PDF and
read the actual sampled techniques before describing an estimator as
full MIS or unbiased. `TracePath` currently uses diffuse bounce
sampling; do not infer full material-BSDF support from the name.

## Stage D — Render scheduling, progressive jobs and output

24. [`Renderer.cppm`](../Renderer.cppm): pixel integration, atomic tile claiming, worker statistics and mode dispatch.
25. [`ProgressiveRenderer.cppm`](../ProgressiveRenderer.cppm): accumulation across progressive passes.
26. [`OfflineRenderJob.cppm`](../OfflineRenderJob.cppm): background task ownership, cancellation, diagnostics and synchronization.
27. [`PreviewWindow.cppm`](../PreviewWindow.cppm): preview/UI boundary rather than image transport physics.
28. [`examples/cli.cpp`](../examples/cli.cpp) and [`examples/preview.cpp`](../examples/preview.cpp): runnable entry points.

**Exercise:** trace one tile index through worker claiming to a unique
pixel rectangle; distinguish per-worker statistics from shared `Film`
writes. Compare a completed offline job against cancellation and
restart paths, without assuming the preview owns native GPU rendering.

## Stage E — Verification, visual output and research claims

29. [`tests/RayTracerTests.cpp`](../tests/RayTracerTests.cpp): geometry/acceleration and render assertions.
30. [`tests/OfflineRenderJobTests.cpp`](../tests/OfflineRenderJobTests.cpp): asynchronous job behavior.
31. [`tests/SharedLightSemanticsTests.cpp`](../tests/SharedLightSemanticsTests.cpp): light-model consistency.
32. [`outputs/gallery`](../outputs/gallery): generated visual examples to inspect with their scene/settings.
33. [`research`](../research): research artifacts and their separate evidence standards.

**Record:** scene, resolution, SPP, maximum depth, seed, integrator,
acceleration mode, threads, render duration and compared image error.
Pretty gallery output is not independent evidence of physically
accurate transport. Source code presence is not evidence that a test
ran on the current host.

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
