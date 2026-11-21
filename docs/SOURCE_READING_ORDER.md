# KairoRayTracer — source reading guide

This **teaching order** follows data/control flow, not compilation order. Read [README](../README.md), [CMakeLists.txt](../CMakeLists.txt) and the applicable test target before trusting a performance claim or assuming a backend is enabled.

1. [RayTracerTypes.cppm](../RayTracerTypes.cppm) — inspect exported declarations, contracts, call paths and tests.
2. [Camera.cppm](../Camera.cppm) — inspect exported declarations, contracts, call paths and tests.
3. [Primitive.cppm](../Primitive.cppm) — inspect exported declarations, contracts, call paths and tests.
4. [Mesh.cppm](../Mesh.cppm) — inspect exported declarations, contracts, call paths and tests.
5. [Scene.cppm](../Scene.cppm) — inspect exported declarations, contracts, call paths and tests.
6. [SceneParser.cppm](../SceneParser.cppm) — inspect exported declarations, contracts, call paths and tests.
7. [Material.cppm](../Material.cppm) — inspect exported declarations, contracts, call paths and tests.
8. [Texture.cppm](../Texture.cppm) — inspect exported declarations, contracts, call paths and tests.
9. [Light.cppm](../Light.cppm) — inspect exported declarations, contracts, call paths and tests.
10. [Sampler.cppm](../Sampler.cppm) — inspect exported declarations, contracts, call paths and tests.
11. [Integrator.cppm](../Integrator.cppm) — inspect exported declarations, contracts, call paths and tests.
12. [WhittedIntegrator.cppm](../WhittedIntegrator.cppm) — inspect exported declarations, contracts, call paths and tests.
13. [PBRIntegrator.cppm](../PBRIntegrator.cppm) — inspect exported declarations, contracts, call paths and tests.
14. [PathIntegrator.cppm](../PathIntegrator.cppm) — inspect exported declarations, contracts, call paths and tests.
15. [Film.cppm](../Film.cppm) — inspect exported declarations, contracts, call paths and tests.
16. [ImageIO.cppm](../ImageIO.cppm) — inspect exported declarations, contracts, call paths and tests.
17. [Renderer.cppm](../Renderer.cppm) — inspect exported declarations, contracts, call paths and tests.
18. [ProgressiveRenderer.cppm](../ProgressiveRenderer.cppm) — inspect exported declarations, contracts, call paths and tests.
19. [KairoRayTracer.cppm](../KairoRayTracer.cppm) — inspect exported declarations, contracts, call paths and tests.
20. [examples/cli.cpp](../examples/cli.cpp) — inspect exported declarations, contracts, call paths and tests.

## How this repository interacts with Kairo

KairoMath and KairoGeometry rays/shapes → KairoSpatial acceleration → Scene/Camera/Material/Light → Sampler and selected Integrator → Film/ImageIO. The CPU Renderer orchestrates tiles/threads; this is not the realtime GPU KairoRenderer.

## Study method

Read the exported C++ module/API, then walk the implementation and its tests. For each function, record its inputs, return type, ownership, error handling, preconditions, numerical assumptions and downstream consumer. Treat configuration/feature flags as constraints on what code is actually compiled.

## Practical exercises

1. Draw the data flow for one smallest supported operation and link every participating module.
2. Find one invalid input and explain whether the API throws, returns an error, or supplies a fallback.
3. Run correctness tests before performance benchmarks; retain compiler/platform/configuration.
4. Locate optional backend paths and distinguish stub/unsupported modes from executed implementations.
5. Identify a dependency ownership boundary and explain when referenced buffers/objects may be destroyed.

## License

The proposed [MIT license](../LICENSE) applies to original project-owned source/documentation. It does not convey rights over external dependencies, datasets, assets or imported code; preserve applicable notices and verify provenance before redistribution.
