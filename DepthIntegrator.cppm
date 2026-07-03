module;

#include <algorithm>
#include <optional>

export module Kairo.Foundation.RayTracer.DepthIntegrator;

import Kairo.Foundation.Geometry.Ray;
import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Types;
import Kairo.Foundation.RayTracer.Scene;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::geometry;

    [[nodiscard]]
    inline Color3f TraceDepth(
        const Scene& scene,
        const Rayf& ray,
        RenderStats* stats)
    {
        const std::optional<SurfaceHit> hit =
            scene.Intersect(ray, stats);

        if (!hit)
        {
            return Color3f::Black();
        }

        if (stats)
        {
            ++stats->HitCount;
        }

        const float range =
            std::max(scene.Settings.DepthFar - scene.Settings.DepthNear, 1.0e-3f);

        const float normalized =
            std::clamp(
                1.0f - ((hit->Distance - scene.Settings.DepthNear) / range),
                0.0f,
                1.0f);

        return { normalized, normalized, normalized };
    }
}
