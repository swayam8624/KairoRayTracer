module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

export module Kairo.Foundation.RayTracer.PathIntegrator;

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
    inline float Random01(
        std::uint32_t& state) noexcept
    {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        return static_cast<float>(state & 0x00ffffffu) /
            static_cast<float>(0x01000000u);
    }

    [[nodiscard]]
    inline Vec3f CosineHemisphereDirection(
        const Vec3f& normal,
        std::uint32_t& rng) noexcept
    {
        const float r1 =
            Random01(rng);

        const float r2 =
            Random01(rng);

        const float phi =
            2.0f * std::numbers::pi_v<float> * r1;

        const float radius =
            std::sqrt(r2);

        const float x =
            std::cos(phi) * radius;

        const float z =
            std::sin(phi) * radius;

        const float y =
            std::sqrt(std::max(0.0f, 1.0f - r2));

        const Vec3f tangent =
            std::abs(normal.y) < 0.999f
                ? SafeNormalize(Cross(Vec3f::Up(), normal), Vec3f::UnitX())
                : Vec3f::UnitX();

        const Vec3f bitangent =
            SafeNormalize(Cross(normal, tangent), Vec3f::Forward());

        return SafeNormalize(
            tangent * x + normal * y + bitangent * z,
            normal);
    }

    [[nodiscard]]
    inline Color3f DirectLambert(
        const Scene& scene,
        const SurfaceHit& hit,
        const Material& material,
        RenderStats* stats)
    {
        Color3f color = Color3f::Black();

        for (const PointLight& light : scene.Lights)
        {
            const Vec3f toLight =
                light.Position - hit.Position;

            const float lightDistance =
                toLight.Length();

            if (lightDistance <= 1.0e-4f)
            {
                continue;
            }

            const Vec3f lightDirection =
                toLight / lightDistance;

            const float nDotL =
                std::max(Dot(hit.Normal, lightDirection), 0.0f);

            if (nDotL <= 0.0f)
            {
                continue;
            }

            if (stats)
            {
                ++stats->ShadowRays;
            }

            const Rayf shadowRay =
                Rayf::FromOriginDirection(
                    hit.Position + hit.Normal * RayBiasForHit(scene.Settings, hit.Distance),
                    lightDirection);

            if (scene.Occluded(shadowRay, lightDistance - RayBiasForHit(scene.Settings, hit.Distance), stats))
            {
                continue;
            }

            const float attenuation =
                light.Intensity /
                std::max(lightDistance * lightDistance, scene.Settings.MinimumLightDistanceSquared);

            color +=
                material.Albedo *
                light.Color *
                (nDotL * attenuation);
        }

        return color;
    }

    [[nodiscard]]
    inline Color3f TracePath(
        const Scene& scene,
        const Rayf& ray,
        std::uint32_t depth,
        std::uint32_t& rng,
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

        const Color3f direct =
            DirectLambert(scene, *hit, material, stats);

        if (depth == scene.Settings.MaxDepth)
        {
            return direct;
        }

        const Vec3f bounceDirection =
            CosineHemisphereDirection(hit->Normal, rng);

        const Rayf bounceRay =
            Rayf::FromOriginDirection(
                hit->Position + hit->Normal * RayBiasForHit(scene.Settings, hit->Distance),
                bounceDirection);

        const Color3f indirect =
            material.Albedo *
            TracePath(scene, bounceRay, depth + 1, rng, stats);

        return direct + indirect;
    }
}
