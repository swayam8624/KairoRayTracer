module;

#include <chrono>
#include <cstdint>
#include <atomic>
#include <algorithm>
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

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::geometry;

    //=========================================================
    // Renderer
    //
    // The renderer is intentionally boring: loop pixels, generate primary rays,
    // ask the active integrator for color, and write the film. This makes it
    // easy to separate bugs in camera math, intersection, shading, or output.
    //=========================================================

    struct RenderResult final
    {
        Film Image;
        RenderStats Stats;
    };

    class Renderer final
    {
    public:
        [[nodiscard]]
        RenderResult Render(
            const Scene& scene) const
        {
            Film film(scene.Settings.Width, scene.Settings.Height);
            RenderStats stats;

            const auto start =
                std::chrono::steady_clock::now();

            const std::uint32_t tileSize =
                scene.Settings.TileSize == 0 ? 16u : scene.Settings.TileSize;

            const std::uint32_t tilesX =
                (scene.Settings.Width + tileSize - 1u) / tileSize;

            const std::uint32_t tilesY =
                (scene.Settings.Height + tileSize - 1u) / tileSize;

            const std::uint32_t tileCount =
                tilesX * tilesY;

            const std::uint32_t hardwareThreads =
                std::max(1u, std::thread::hardware_concurrency());

            const std::uint32_t workerCount =
                std::max(
                    1u,
                    std::min(
                        scene.Settings.ThreadCount == 0 ? hardwareThreads : scene.Settings.ThreadCount,
                        tileCount == 0 ? 1u : tileCount));

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
                        const PixelSample pixelSample =
                            StratifiedPixelSample(
                                x,
                                y,
                                sample,
                                scene.Settings.SamplesPerPixel);

                        const float u =
                            (static_cast<float>(x) + pixelSample.dx) /
                            static_cast<float>(scene.Settings.Width);

                        const float v =
                            (static_cast<float>(y) + pixelSample.dy) /
                            static_cast<float>(scene.Settings.Height);

                        const Rayf ray =
                            scene.MainCamera.GenerateRay(u, v);

                        ++localStats.PrimaryRays;
                        accumulated += Trace(scene, ray, &localStats);
                    }

                    film.SetPixel(
                        x,
                        y,
                        accumulated / static_cast<float>(scene.Settings.SamplesPerPixel));
                };

            auto renderTiles =
                [&](std::uint32_t workerIndex)
                {
                    RenderStats& localStats =
                        workerStats[workerIndex];

                    while (true)
                    {
                        const std::uint32_t tileIndex =
                            nextTile.fetch_add(1u, std::memory_order_relaxed);

                        if (tileIndex >= tileCount)
                        {
                            break;
                        }

                        const std::uint32_t tileX =
                            tileIndex % tilesX;

                        const std::uint32_t tileY =
                            tileIndex / tilesX;

                        const std::uint32_t beginX =
                            tileX * tileSize;

                        const std::uint32_t beginY =
                            tileY * tileSize;

                        const std::uint32_t endX =
                            std::min(beginX + tileSize, scene.Settings.Width);

                        const std::uint32_t endY =
                            std::min(beginY + tileSize, scene.Settings.Height);

                        for (std::uint32_t y = beginY; y < endY; ++y)
                        {
                            for (std::uint32_t x = beginX; x < endX; ++x)
                            {
                                renderPixel(x, y, localStats);
                            }
                        }
                    }
                };

            for (std::uint32_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
            {
                workers.emplace_back(renderTiles, workerIndex);
            }

            for (std::thread& worker : workers)
            {
                worker.join();
            }

            for (const RenderStats& localStats : workerStats)
            {
                Accumulate(stats, localStats);
            }

            const auto end =
                std::chrono::steady_clock::now();

            stats.RenderMilliseconds =
                std::chrono::duration<double, std::milli>(end - start).count();

            FinalizeDerivedStats(stats);

            return { std::move(film), stats };
        }

    private:
        [[nodiscard]]
        static Color3f Trace(
            const Scene& scene,
            const Rayf& ray,
            RenderStats* stats)
        {
            switch (scene.Settings.Mode)
            {
            case RenderMode::Normal:
                return TraceNormal(scene, ray, stats);
            case RenderMode::Depth:
                return TraceDepth(scene, ray, stats);
            case RenderMode::Whitted:
                return TraceWhitted(scene, ray, 0, stats);
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
