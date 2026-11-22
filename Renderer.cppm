// TEACHING MODULE: Renderer is the CPU image-formation driver. For each
// pixel/sample it obtains a deterministic sample offset from Sampler, asks
// Camera for a primary ray, selects an integrator, and writes averaged
// radiance to Film. The Scene owns geometry/BVH/materials and handles hits;
// Film/ImageIO own image data/output, not a GPU framebuffer.
//
// Work is partitioned into disjoint rectangular tiles. Threads claim tile
// indices via an atomic counter and only touch their own pixel coordinates.
// Each worker accumulates RenderStats locally, then the main thread merges
// stats after joining. These ownership boundaries matter for safe edits:
// sharing a mutable per-pixel RNG or unsynchronized film writes across tiles
// would invalidate the current design.
//
// RenderMode dispatch includes normal/depth/debug, Whitted, direct PBR and
// path tracing. Primary fog runs after selected beauty integrators and adds
// an uncounted intersection for diagnostic-statistics compatibility.
module;

#include <chrono>
#include <cstdint>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

export module Kairo.Foundation.RayTracer.Renderer;

import Kairo.Foundation.Geometry.Ray;
import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Types;
import Kairo.Foundation.RayTracer.Film;
import Kairo.Foundation.RayTracer.Scene;
import Kairo.Foundation.RayTracer.Sampler;
import Kairo.Foundation.RayTracer.NormalIntegrator;
import Kairo.Foundation.RayTracer.DepthIntegrator;
import Kairo.Foundation.RayTracer.WhittedIntegrator;
import Kairo.Foundation.RayTracer.PBRIntegrator;
import Kairo.Foundation.RayTracer.PathIntegrator;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::geometry;

    struct RenderResult final
    {
        Film Image;
        RenderStats Stats;
    };

    class Renderer final
    {
    public:
        [[nodiscard]]
        // Render settings determine image dimensions, samples, tile size,
        // CPU thread count and chosen integrator. A fresh Film and worker-
        // local statistics are created for this invocation.
        RenderResult Render(
            const Scene& scene) const
        {
            ValidateRenderSettings(scene.Settings);

            Film film(scene.Settings.Width, scene.Settings.Height);
            RenderStats stats;

            const auto start = std::chrono::steady_clock::now();
            const std::uint32_t tileSize = scene.Settings.TileSize;
            const std::uint32_t tilesX =
                (scene.Settings.Width + tileSize - 1u) / tileSize;
            const std::uint32_t tilesY =
                (scene.Settings.Height + tileSize - 1u) / tileSize;
            const std::uint32_t tileCount = tilesX * tilesY;
            const std::uint32_t hardwareThreads =
                std::max(1u, std::thread::hardware_concurrency());
            const std::uint32_t workerCount =
                std::max(
                    1u,
                    std::min(
                        scene.Settings.ThreadCount == 0 ? hardwareThreads : scene.Settings.ThreadCount,
                        tileCount == 0 ? 1u : tileCount));

            // Workers claim unique tile indices; each tile covers a
            // nonoverlapping pixel rectangle, so a shared Film can be
            // populated without two workers writing the same pixel.
            std::atomic<std::uint32_t> nextTile = 0;
            std::vector<RenderStats> workerStats(workerCount);
            std::vector<std::thread> workers;
            workers.reserve(workerCount);

            auto renderPixel =
                [&](std::uint32_t x, std::uint32_t y, RenderStats& localStats)
                {
                    Color3f accumulated = Color3f::Black();

                    for (std::uint32_t sample = 0; sample < scene.Settings.SamplesPerPixel; ++sample)
                    {
                        // Sample offsets depend on coordinates, sample index and
            // configured seed, not the executing worker index.
            const PixelSample pixelSample =
                            StratifiedPixelSample(
                                x,
                                y,
                                sample,
                                scene.Settings.SamplesPerPixel,
                                scene.Settings.SampleSeed);

                        const float u =
                            (static_cast<float>(x) + pixelSample.dx) /
                            static_cast<float>(scene.Settings.Width);
                        const float v =
                            (static_cast<float>(y) + pixelSample.dy) /
                            static_cast<float>(scene.Settings.Height);

                        const Rayf ray = scene.MainCamera.GenerateRay(u, v);

                        ++localStats.PrimaryRays;
                        accumulated += Trace(scene, ray, x, y, sample, &localStats);
                    }

                    film.SetPixel(
                        x,
                        y,
                        accumulated / static_cast<float>(scene.Settings.SamplesPerPixel));
                };

            auto renderTiles =
                [&](std::uint32_t workerIndex)
                {
                    RenderStats& localStats = workerStats[workerIndex];

                    while (true)
                    {
                        const std::uint32_t tileIndex =
                            nextTile.fetch_add(1u, std::memory_order_relaxed);

                        if (tileIndex >= tileCount)
                            break;

                        const std::uint32_t tileX = tileIndex % tilesX;
                        const std::uint32_t tileY = tileIndex / tilesX;
                        const std::uint32_t beginX = tileX * tileSize;
                        const std::uint32_t beginY = tileY * tileSize;
                        const std::uint32_t endX =
                            std::min(beginX + tileSize, scene.Settings.Width);
                        const std::uint32_t endY =
                            std::min(beginY + tileSize, scene.Settings.Height);

                        for (std::uint32_t y = beginY; y < endY; ++y)
                        {
                            for (std::uint32_t x = beginX; x < endX; ++x)
                                renderPixel(x, y, localStats);
                        }
                    }
                };

            for (std::uint32_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
                workers.emplace_back(renderTiles, workerIndex);

            for (std::thread& worker : workers)
                worker.join();

            for (const RenderStats& localStats : workerStats)
                Accumulate(stats, localStats);

            const auto end = std::chrono::steady_clock::now();
            stats.RenderMilliseconds =
                std::chrono::duration<double, std::milli>(end - start).count();
            FinalizeDerivedStats(stats);

            return { std::move(film), stats };
        }

    private:
        [[nodiscard]]
        // Fog is composed at the primary-ray layer for specific beauty
        // modes, not by modifying Scene or each integrator separately.
        static Color3f ApplyPrimaryFog(
            const Scene& scene,
            const Rayf& ray,
            const Color3f& color)
        {
            if (scene.Settings.Fog == FogMode::Disabled)
                return color;

            // Deliberately do not accumulate stats for this visibility query:
            // fog is a post-integrator primary-ray composition step, not an
            // additional authored ray. This keeps existing profiling semantics
            // stable while all beauty integrators share identical fog behavior.
            const auto hit = scene.Intersect(ray, nullptr);
            const float distance =
                hit ? hit->Distance : std::numeric_limits<float>::infinity();
            return ApplyFog(scene.Settings, color, distance);
        }

        [[nodiscard]]
        // Dispatch rendering policy to specialized integrator modules.
        // A new RenderMode belongs here and in settings validation/tests;
        // adding its enum alone is not sufficient.
        static Color3f Trace(
            const Scene& scene,
            const Rayf& ray,
            std::uint32_t x,
            std::uint32_t y,
            std::uint32_t sample,
            RenderStats* stats)
        {
            switch (scene.Settings.Mode)
            {
            case RenderMode::Normal:
                return TraceNormal(scene, ray, stats);
            case RenderMode::Depth:
                return TraceDepth(scene, ray, stats);
            case RenderMode::Whitted:
                return ApplyPrimaryFog(
                    scene, ray, TraceWhitted(scene, ray, 0, stats));
            case RenderMode::PBR:
                return ApplyPrimaryFog(
                    scene, ray, TracePBRDirect(scene, ray, stats));
            case RenderMode::Path:
            {
                std::uint32_t rng =
                    x * 73856093u ^
                    y * 19349663u ^
                    sample * 83492791u ^
                    scene.Settings.SampleSeed * 2654435761u ^
                    0x9e3779b9u;

                return ApplyPrimaryFog(
                    scene, ray, TracePath(scene, ray, 0, rng, stats));
            }
            case RenderMode::ShadowMask:
                return TraceShadowMask(scene, ray, stats);
            case RenderMode::BVHHeatmap:
                return TraceBVHHeatmap(scene, ray, stats);
            case RenderMode::Albedo:
            case RenderMode::PrimitiveID:
            case RenderMode::UV:
            case RenderMode::Barycentric:
            case RenderMode::AccelerationDifference:
                return TraceDebugMode(scene, ray, stats);
            }

            return Color3f::Black();
        }
    };
}
