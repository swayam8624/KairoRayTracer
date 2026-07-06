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
import Kairo.Foundation.RayTracer.Shading;

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
    inline float PowerHeuristic(
        float pdfA,
        float pdfB) noexcept
    {
        const float a2 =
            pdfA * pdfA;

        const float b2 =
            pdfB * pdfB;

        return a2 / std::max(a2 + b2, 1.0e-8f);
    }

    [[nodiscard]]
    inline Color3f DirectLambert(
        const Scene& scene,
        const SurfaceHit& hit,
        const Color3f& albedo,
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
                albedo *
                light.Color *
                (nDotL * attenuation);
        }

        for (const AreaLight& light : scene.AreaLights)
        {
            const std::uint32_t samples =
                std::max(1u, light.Samples);

            for (std::uint32_t sample = 0; sample < samples; ++sample)
            {
                const float u =
                    (static_cast<float>(sample % 2u) + 0.5f) / 2.0f - 0.5f;

                const float v =
                    (static_cast<float>((sample / 2u) % 2u) + 0.5f) / 2.0f - 0.5f;

                const Vec3f lightPosition =
                    light.Position + light.U * u + light.V * v;

                const Vec3f toLight =
                    lightPosition - hit.Position;

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

                const Vec3f lightNormal =
                    SafeNormalize(Cross(light.U, light.V), -lightDirection);

                const float lightFacing =
                    std::max(Dot(lightNormal, -lightDirection), 0.0f);

                const float lightArea =
                    std::max(Cross(light.U, light.V).Length(), 1.0e-5f);

                const float lightPdf =
                    lightDistance * lightDistance /
                    std::max(lightArea * lightFacing, 1.0e-5f);

                const float bsdfPdf =
                    nDotL / std::numbers::pi_v<float>;

                const float misWeight =
                    PowerHeuristic(lightPdf, bsdfPdf);

                color +=
                    albedo *
                    light.Color *
                    (nDotL * attenuation * misWeight / static_cast<float>(samples));
            }
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

            return scene.SampleEnvironment(ray.Direction);
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

        const Color3f albedo =
            EvaluateAlbedo(scene, *hit);

        if (material.Type == MaterialType::Emissive)
        {
            return material.Emission;
        }

        const Color3f direct =
            DirectLambert(scene, *hit, albedo, stats);

        if (depth == scene.Settings.MaxDepth)
        {
            return direct;
        }

        float survivalProbability =
            std::max({ albedo.r, albedo.g, albedo.b, 0.15f });

        survivalProbability =
            std::min(survivalProbability, 0.95f);

        if (depth >= 2 && Random01(rng) > survivalProbability)
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
            albedo *
            (TracePath(scene, bounceRay, depth + 1, rng, stats) / survivalProbability);

        return direct + indirect;
    }
}
