module;

#include <chrono>
#include <cstdint>

export module Kairo.Foundation.RayTracer.Renderer;

import Kairo.Foundation.Geometry.Ray;
import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Types;
import Kairo.Foundation.RayTracer.Film;
import Kairo.Foundation.RayTracer.Scene;
import Kairo.Foundation.RayTracer.NormalIntegrator;
import Kairo.Foundation.RayTracer.DepthIntegrator;
import Kairo.Foundation.RayTracer.WhittedIntegrator;

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
        RenderResult Render(
            const Scene& scene) const
        {
            Film film(scene.Settings.Width, scene.Settings.Height);
            RenderStats stats;

            const auto start =
                std::chrono::steady_clock::now();

            for (std::uint32_t y = 0; y < scene.Settings.Height; ++y)
            {
                for (std::uint32_t x = 0; x < scene.Settings.Width; ++x)
                {
                    Color3f accumulated = Color3f::Black();

                    for (std::uint32_t sample = 0; sample < scene.Settings.SamplesPerPixel; ++sample)
                    {
                        (void)sample;

                        const float u =
                            (static_cast<float>(x) + 0.5f) /
                            static_cast<float>(scene.Settings.Width);

                        const float v =
                            (static_cast<float>(y) + 0.5f) /
                            static_cast<float>(scene.Settings.Height);

                        const Rayf ray =
                            scene.MainCamera.GenerateRay(u, v);

                        ++stats.PrimaryRays;
                        accumulated += Trace(scene, ray, &stats);
                    }

                    film.SetPixel(
                        x,
                        y,
                        accumulated / static_cast<float>(scene.Settings.SamplesPerPixel));
                }
            }

            const auto end =
                std::chrono::steady_clock::now();

            stats.RenderMilliseconds =
                std::chrono::duration<double, std::milli>(end - start).count();

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
            }

            return Color3f::Black();
        }
    };
}
