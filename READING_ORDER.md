# KairoRayTracer: linked source reading guide

Study the camera/film/color contracts and geometric primitives, then material and light behavior, surface intersection/acceleration, integrators/path tracing, image output and visual tests. Unlike KairoRenderer's Metal/Vulkan backend, this is a standalone CPU/offline renderer and should be benchmarked accordingly.

## Concepts and cross-module APIs

Follow Camera -> Ray -> intersection/BVH -> Material/Light -> radiance estimate -> Film -> ImageIO. Study Monte Carlo estimators, Russian roulette/path depth, numerical ray epsilon, BSDF/PDF correctness and progressive accumulation before interpreting images as verified physical accuracy.

## Source order (32 files)

1. [`KairoRayTracer.cppm`](KairoRayTracer.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
2. [`Camera.cppm`](Camera.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
3. [`Film.cppm`](Film.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
4. [`Color.cppm`](Color.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
5. [`Material.cppm`](Material.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
6. [`Light.cppm`](Light.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
7. [`Environment.cppm`](Environment.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
8. [`Scene.cppm`](Scene.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
9. [`DepthIntegrator.cppm`](DepthIntegrator.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
10. [`Integrator.cppm`](Integrator.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
11. [`NormalIntegrator.cppm`](NormalIntegrator.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
12. [`PathIntegrator.cppm`](PathIntegrator.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
13. [`PBRIntegrator.cppm`](PBRIntegrator.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
14. [`WhittedIntegrator.cppm`](WhittedIntegrator.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
15. [`ImageIO.cppm`](ImageIO.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
16. [`examples/cli.cpp`](examples/cli.cpp) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
17. [`examples/preview.cpp`](examples/preview.cpp) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
18. [`Mesh.cppm`](Mesh.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
19. [`OBJLoader.cppm`](OBJLoader.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
20. [`OfflineRenderJob.cppm`](OfflineRenderJob.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
21. [`PreviewWindow.cppm`](PreviewWindow.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
22. [`Primitive.cppm`](Primitive.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
23. [`ProgressiveRenderer.cppm`](ProgressiveRenderer.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
24. [`RayTracerTypes.cppm`](RayTracerTypes.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
25. [`Renderer.cppm`](Renderer.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
26. [`Sampler.cppm`](Sampler.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
27. [`SceneParser.cppm`](SceneParser.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
28. [`Shading.cppm`](Shading.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
29. [`tests/OfflineRenderJobTests.cpp`](tests/OfflineRenderJobTests.cpp) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
30. [`tests/RayTracerTests.cpp`](tests/RayTracerTests.cpp) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
31. [`tests/SharedLightSemanticsTests.cpp`](tests/SharedLightSemanticsTests.cpp) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
32. [`Texture.cppm`](Texture.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.

## Reproduce and extend

Use [README.md](README.md) for setup and the relevant build-system/tests for independent verification. A green isolated build is not proof of the complete 27-module Kairo workspace, whose heads must pass [KairoGameEngine acceptance](https://github.com/swayam8624/KairoGameEngine). Original code is [MIT-licensed](LICENSE) for learning, reuse and further development; third-party dependencies retain their licenses. The index is an initial teaching map, not complete per-function annotation.
