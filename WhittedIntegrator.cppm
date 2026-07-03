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
    inline Color3f HashIDColor(
        std::uint32_t id) noexcept
    {
        id ^= id >> 16u;
        id *= 0x7feb352du;
        id ^= id >> 15u;
        id *= 0x846ca68bu;
        id ^= id >> 16u;

        return
        {
            static_cast<float>((id >> 0u) & 0xffu) / 255.0f,
            static_cast<float>((id >> 8u) & 0xffu) / 255.0f,
            static_cast<float>((id >> 16u) & 0xffu) / 255.0f
        };
    }

    [[nodiscard]]
    inline float SchlickFresnel(
        float cosine,
        float ior) noexcept
    {
        const float r0Base =
            (1.0f - ior) / (1.0f + ior);

        const float r0 =
            r0Base * r0Base;

        const float oneMinusCosine =
            1.0f - std::clamp(cosine, 0.0f, 1.0f);

        return r0 + (1.0f - r0) * std::pow(oneMinusCosine, 5.0f);
    }

    // Whitted tracing is classic recursive ray tracing:
    // primary ray -> nearest hit -> direct light/shadow -> optional mirror ray.
    // It is not path tracing; it does not sample indirect diffuse bounces.
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
            if (stats)
            {
                ++stats->MissCount;
            }

            return scene.Settings.Background;
        }

        if (stats)
        {
            ++stats->HitCount;
            stats->MaxRecursionDepthReached =
                stats->MaxRecursionDepthReached > depth
                    ? stats->MaxRecursionDepthReached
                    : depth;
        }

        const Material& material =
            scene.Materials.at(hit->MaterialIndex);

        if (material.Type == MaterialType::Emissive)
        {
            return material.Emission;
        }

        if (material.Type == MaterialType::Mirror)
        {
            // Perfect mirrors do not use local diffuse lighting. The outgoing
            // color is the reflected ray result tinted by the material albedo.
            const Vec3f reflectionDirection =
                SafeNormalize(
                    Reflect(ray.Direction, hit->Normal),
                    hit->Normal);

            const Rayf reflectionRay =
                Rayf::FromOriginDirection(
                    hit->Position + hit->Normal * RayBiasForHit(scene.Settings, hit->Distance),
                    reflectionDirection);

            if (stats)
            {
                ++stats->ReflectionRays;
            }

            return material.Albedo *
                TraceWhitted(scene, reflectionRay, depth + 1, stats);
        }

        if (material.Type == MaterialType::Glass)
        {
            Vec3f normal =
                hit->Normal;

            float etaRatio =
                1.0f / material.IOR;

            float cosine =
                std::clamp(Dot(-ray.Direction, normal), 0.0f, 1.0f);

            if (Dot(ray.Direction, normal) > 0.0f)
            {
                normal = -normal;
                etaRatio = material.IOR;
                cosine = std::clamp(Dot(-ray.Direction, normal), 0.0f, 1.0f);
            }

            const Vec3f reflectionDirection =
                SafeNormalize(
                    Reflect(ray.Direction, normal),
                    normal);

            const Vec3f refractionDirection =
                Refract(ray.Direction, normal, etaRatio);

            const float fresnel =
                refractionDirection.LengthSquared() <= 1.0e-8f
                    ? 1.0f
                    : SchlickFresnel(cosine, material.IOR);

            const float bias =
                RayBiasForHit(scene.Settings, hit->Distance);

            const Color3f reflected =
                TraceWhitted(
                    scene,
                    Rayf::FromOriginDirection(hit->Position + normal * bias, reflectionDirection),
                    depth + 1,
                    stats);

            Color3f refracted =
                Color3f::Black();

            if (fresnel < 1.0f)
            {
                refracted =
                    TraceWhitted(
                        scene,
                        Rayf::FromOriginDirection(hit->Position - normal * bias, SafeNormalize(refractionDirection, ray.Direction)),
                        depth + 1,
                        stats);
            }

            if (stats)
            {
                stats->ReflectionRays += 2u;
            }

            return material.Albedo *
                (reflected * fresnel + refracted * (1.0f - fresnel));
        }

        Color3f color =
            material.Albedo * 0.10f;

        for (const PointLight& light : scene.Lights)
        {
            // Lambertian direct light: albedo * lightColor * max(N dot L, 0).
            // The shadow ray decides whether this light is visible from the hit.
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
                    hit->Position + hit->Normal * RayBiasForHit(scene.Settings, hit->Distance),
                    lightDirection);

            if (stats)
            {
                ++stats->ShadowRays;
            }

            if (scene.Occluded(shadowRay, lightDistance - RayBiasForHit(scene.Settings, hit->Distance), stats))
            {
                continue;
            }

            const float attenuation =
                light.Intensity / std::max(lightDistance * lightDistance, scene.Settings.MinimumLightDistanceSquared);

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
        // White means at least one light is visible. Dark gray means every light
        // was blocked. This is a focused debug view for shadow-ray behavior.
        const std::optional<SurfaceHit> hit =
            scene.Intersect(ray, stats);

        if (!hit)
        {
            if (stats)
            {
                ++stats->MissCount;
            }

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
                    hit->Position + hit->Normal * RayBiasForHit(scene.Settings, hit->Distance),
                    lightDirection);

            if (!scene.Occluded(shadowRay, lightDistance - RayBiasForHit(scene.Settings, hit->Distance), stats))
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
        // Heatmaps turn traversal work into color. Brighter red means more BVH
        // nodes were visited for that pixel, useful when testing scene layout or
        // split heuristics.
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
            if (stats)
            {
                ++stats->MissCount;
            }

            return Color3f::Black();
        }

        const float heat =
            std::clamp(
                static_cast<float>(localStats.BVHVisitedNodes) / 32.0f,
                0.0f,
                1.0f);

        return { heat, heat * 0.35f, 1.0f - heat };
    }

    [[nodiscard]]
    inline Color3f TraceDebugMode(
        const Scene& scene,
        const Rayf& ray,
        RenderStats* stats)
    {
        const std::optional<SurfaceHit> hit =
            scene.Intersect(ray, stats);

        if (!hit)
        {
            if (stats)
            {
                ++stats->MissCount;
            }

            return Color3f::Black();
        }

        if (stats)
        {
            ++stats->HitCount;
        }

        if (scene.Settings.Mode == RenderMode::AccelerationDifference)
        {
            const std::optional<SurfaceHit> brute =
                scene.BruteForceIntersect(ray);

            if (!brute)
            {
                return { 0.0f, 0.0f, 1.0f };
            }

            const bool same =
                brute->PrimitiveIndex == hit->PrimitiveIndex &&
                std::abs(brute->Distance - hit->Distance) <= 1.0e-4f &&
                brute->MaterialIndex == hit->MaterialIndex &&
                std::abs(brute->UV.x - hit->UV.x) <= 1.0e-4f &&
                std::abs(brute->UV.y - hit->UV.y) <= 1.0e-4f;

            return same
                ? Color3f::Black()
                : Color3f{ 1.0f, 0.0f, 0.0f };
        }

        if (scene.Settings.Mode == RenderMode::Albedo)
        {
            return scene.Materials.at(hit->MaterialIndex).Albedo;
        }

        if (scene.Settings.Mode == RenderMode::PrimitiveID)
        {
            return HashIDColor(hit->PrimitiveIndex + 1u);
        }

        if (scene.Settings.Mode == RenderMode::UV)
        {
            return
            {
                std::clamp(hit->UV.x, 0.0f, 1.0f),
                std::clamp(hit->UV.y, 0.0f, 1.0f),
                0.0f
            };
        }

        if (scene.Settings.Mode == RenderMode::Barycentric)
        {
            const float u =
                std::clamp(hit->UV.x, 0.0f, 1.0f);

            const float v =
                std::clamp(hit->UV.y, 0.0f, 1.0f);

            return
            {
                std::clamp(1.0f - u - v, 0.0f, 1.0f),
                u,
                v
            };
        }

        return Color3f::Black();
    }
}
