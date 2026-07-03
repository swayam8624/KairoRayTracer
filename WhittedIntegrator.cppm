module;

#include <algorithm>
#include <cmath>
#include <optional>

export module Kairo.Foundation.RayTracer.WhittedIntegrator;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Ray;
import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Types;
import Kairo.Foundation.RayTracer.Material;
import Kairo.Foundation.RayTracer.Light;
import Kairo.Foundation.RayTracer.Scene;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;
    using namespace kairo::foundation::geometry;

    [[nodiscard]]
    inline Color3f TraceWhitted(
        const Scene& scene,
        const Rayf& ray,
        std::uint32_t depth,
        RenderStats* stats)
    {
        if (depth > scene.Settings.MaxDepth)
        {
            return Color3f::Black();
        }

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

        const Material& material =
            scene.Materials.at(hit->MaterialIndex);

        if (material.Type == MaterialType::Emissive)
        {
            return material.Emission;
        }

        if (material.Type == MaterialType::Mirror)
        {
            const Vec3f reflectionDirection =
                SafeNormalize(
                    Reflect(ray.Direction, hit->Normal),
                    hit->Normal);

            const Rayf reflectionRay =
                Rayf::FromOriginDirection(
                    hit->Position + hit->Normal * scene.Settings.RayBias,
                    reflectionDirection);

            if (stats)
            {
                ++stats->ReflectionRays;
            }

            return material.Albedo *
                TraceWhitted(scene, reflectionRay, depth + 1, stats);
        }

        Color3f color =
            material.Albedo * 0.035f;

        for (const PointLight& light : scene.Lights)
        {
            const Vec3f toLight =
                light.Position - hit->Position;

            const float lightDistance =
                toLight.Length();

            if (lightDistance <= 1.0e-4f)
            {
                continue;
            }

            const Vec3f lightDirection =
                toLight / lightDistance;

            const float nDotL =
                std::max(Dot(hit->Normal, lightDirection), 0.0f);

            if (nDotL <= 0.0f)
            {
                continue;
            }

            const Rayf shadowRay =
                Rayf::FromOriginDirection(
                    hit->Position + hit->Normal * scene.Settings.RayBias,
                    lightDirection);

            if (stats)
            {
                ++stats->ShadowRays;
            }

            if (scene.Occluded(shadowRay, lightDistance - scene.Settings.RayBias, stats))
            {
                continue;
            }

            const float attenuation =
                light.Intensity / std::max(lightDistance * lightDistance, 1.0f);

            color +=
                material.Albedo *
                light.Color *
                (nDotL * attenuation);
        }

        return color;
    }

    [[nodiscard]]
    inline Color3f TraceShadowMask(
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

        if (scene.Lights.empty())
        {
            return Color3f::White();
        }

        for (const PointLight& light : scene.Lights)
        {
            const Vec3f toLight =
                light.Position - hit->Position;

            const float lightDistance =
                toLight.Length();

            const Vec3f lightDirection =
                SafeNormalize(toLight, Vec3f::Up());

            const Rayf shadowRay =
                Rayf::FromOriginDirection(
                    hit->Position + hit->Normal * scene.Settings.RayBias,
                    lightDirection);

            if (!scene.Occluded(shadowRay, lightDistance - scene.Settings.RayBias, stats))
            {
                return Color3f::White();
            }
        }

        return { 0.08f, 0.08f, 0.08f };
    }

    [[nodiscard]]
    inline Color3f TraceBVHHeatmap(
        const Scene& scene,
        const Rayf& ray,
        RenderStats* stats)
    {
        RenderStats localStats;
        const std::optional<SurfaceHit> hit =
            scene.Intersect(ray, &localStats);

        if (stats)
        {
            stats->BVHVisitedNodes += localStats.BVHVisitedNodes;
            stats->BVHTestedPrimitives += localStats.BVHTestedPrimitives;
        }

        if (!hit)
        {
            return Color3f::Black();
        }

        const float heat =
            std::clamp(
                static_cast<float>(localStats.BVHVisitedNodes) / 32.0f,
                0.0f,
                1.0f);

        return { heat, heat * 0.35f, 1.0f - heat };
    }
}
