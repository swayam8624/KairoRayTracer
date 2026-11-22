// TEACHING ENTRY POINT: This public C++23 module aggregates a CPU ray-tracing
// pipeline. `export import` republishes narrow interfaces; it does not
// execute a render, allocate an implicit GPU device or construct a scene.
//
// Data ownership flows from SceneParser/Scene (primitives, materials, lights,
// camera) through Scene::BuildAcceleration (KairoSpatial BVH) to Renderer,
// which samples camera rays and selects an Integrator. Scene::Intersect calls
// exact Geometry ray tests after BVH candidate traversal. Film accumulates
// results, and ImageIO serializes pixels; ProgressiveRenderer and
// OfflineRenderJob manage multi-step/asynchronous CPU rendering.
//
// KairoMath defines vector/transform conventions, KairoGeometry implements
// ray/shape intersection, KairoSpatial owns acceleration structures; this
// repository owns rendering semantics, shading and output. It is independent
// of KairoRenderer's native GPU/swapchain lifetime.
//
// Read docs/SOURCE_READING_ORDER.md in stages, and verify expected image
// results in tests/RayTracerTests.cpp before relying on performance claims.
export module Kairo.Foundation.RayTracer;

// Umbrella module for users who want the complete V1 ray tracer surface through
// one import. Individual modules remain available for focused builds/tests.
export import Kairo.Foundation.RayTracer.Color;
export import Kairo.Foundation.RayTracer.Types;
export import Kairo.Foundation.RayTracer.Film;
export import Kairo.Foundation.RayTracer.ImageIO;
export import Kairo.Foundation.RayTracer.Texture;
export import Kairo.Foundation.RayTracer.Environment;
export import Kairo.Foundation.RayTracer.Camera;
export import Kairo.Foundation.RayTracer.Material;
export import Kairo.Foundation.RayTracer.Light;
export import Kairo.Foundation.RayTracer.Primitive;
export import Kairo.Foundation.RayTracer.Mesh;
export import Kairo.Foundation.RayTracer.OBJLoader;
export import Kairo.Foundation.RayTracer.Scene;
export import Kairo.Foundation.RayTracer.Shading;
export import Kairo.Foundation.RayTracer.SceneParser;
export import Kairo.Foundation.RayTracer.Sampler;
export import Kairo.Foundation.RayTracer.Integrator;
export import Kairo.Foundation.RayTracer.NormalIntegrator;
export import Kairo.Foundation.RayTracer.DepthIntegrator;
export import Kairo.Foundation.RayTracer.WhittedIntegrator;
export import Kairo.Foundation.RayTracer.PBRIntegrator;
export import Kairo.Foundation.RayTracer.PathIntegrator;
export import Kairo.Foundation.RayTracer.Renderer;
export import Kairo.Foundation.RayTracer.ProgressiveRenderer;
export import Kairo.Foundation.RayTracer.OfflineRenderJob;
