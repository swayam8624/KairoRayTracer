module;

#include <optional>

export module Kairo.Foundation.RayTracer.NormalIntegrator;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Ray;
import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Types;
import Kairo.Foundation.RayTracer.Scene;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;
    using namespace kairo::foundation::geometry;

    // Normal rendering maps surface normal components from [-1, 1] to [0, 1].
    // It is the fastest way to see wrong winding, bad transforms, or incorrect
    // sphere normals before lighting makes the problem harder to read.
    [[nodiscard]]
    inline Color3f TraceNormal(
        const Scene& scene,
        const Rayf& ray,
        RenderStats* stats)
    {
        const std::optional<SurfaceHit> hit =
            scene.Intersect(ray, stats);

        if (!hit)
        {
            return scene.Settings.Background;
        }

        if (stats)
        {
            ++stats->HitCount;
        }

        return
        {
            hit->Normal.x * 0.5f + 0.5f,
            hit->Normal.y * 0.5f + 0.5f,
            hit->Normal.z * 0.5f + 0.5f
        };
    }
}
