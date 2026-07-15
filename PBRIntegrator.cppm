module;

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

export module Kairo.Foundation.RayTracer.PBRIntegrator;

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
    inline float DistributionGGX(
        float nDotH,
        float roughness) noexcept
    {
        const float a =
            roughness * roughness;

        const float a2 =
            a * a;

        const float denominator =
            nDotH * nDotH * (a2 - 1.0f) + 1.0f;

        return a2 /
            std::max(std::numbers::pi_v<float> * denominator * denominator, 1.0e-5f);
    }

    [[nodiscard]]
    inline float GeometrySchlickGGX(
        float nDotV,
        float roughness) noexcept
    {
        const float r =
            roughness + 1.0f;

        const float k =
            (r * r) / 8.0f;

        return nDotV /
            std::max(nDotV * (1.0f - k) + k, 1.0e-5f);
    }

    [[nodiscard]]
    inline Color3f FresnelSchlick(
        float cosine,
        const Color3f& f0) noexcept
    {
        const float factor =
            std::pow(1.0f - std::clamp(cosine, 0.0f, 1.0f), 5.0f);

        return f0 + (Color3f::White() - f0) * factor;
    }

    [[nodiscard]]
    inline Color3f TracePBRDirect(
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

            return scene.SampleEnvironment(ray.Direction);
        }

        if (stats)
        {
            ++stats->HitCount;
        }

        const Material& material =
            scene.Materials.at(hit->MaterialIndex);

        const Color3f albedo =
            EvaluateAlbedo(scene, *hit);

        if (material.Type == MaterialType::Emissive)
        {
            return material.Emission;
        }

        const Vec3f viewDirection =
            SafeNormalize(-ray.Direction, Vec3f::Forward());

        const float roughness =
            std::clamp(material.Roughness, 0.04f, 1.0f);

        const float metallic =
            std::clamp(material.Metallic, 0.0f, 1.0f);

        const Color3f dielectricF0 =
            { 0.04f, 0.04f, 0.04f };

        const Color3f f0 =
            dielectricF0 * (1.0f - metallic) + albedo * metallic;

        Color3f color =
            albedo * 0.025f;

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

            const float nDotV =
                std::max(Dot(hit->Normal, viewDirection), 0.0f);

            if (nDotL <= 0.0f || nDotV <= 0.0f)
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

            const Vec3f halfVector =
                SafeNormalize(viewDirection + lightDirection, hit->Normal);

            const float nDotH =
                std::max(Dot(hit->Normal, halfVector), 0.0f);

            const float hDotV =
                std::max(Dot(halfVector, viewDirection), 0.0f);

            const float distribution =
                DistributionGGX(nDotH, roughness);

            const float geometry =
                GeometrySchlickGGX(nDotV, roughness) *
                GeometrySchlickGGX(nDotL, roughness);

            const Color3f fresnel =
                FresnelSchlick(hDotV, f0);

            const Color3f specular =
                fresnel *
                (distribution * geometry /
                    std::max(4.0f * nDotV * nDotL, 1.0e-5f));

            const Color3f diffuse =
                albedo *
                ((1.0f - metallic) / std::numbers::pi_v<float>);

            const float attenuation =
                light.Intensity /
                std::max(lightDistance * lightDistance, scene.Settings.MinimumLightDistanceSquared);

            color +=
                (diffuse + specular) *
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
                    lightPosition - hit->Position;

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

                const float nDotV =
                    std::max(Dot(hit->Normal, viewDirection), 0.0f);

                if (nDotL <= 0.0f || nDotV <= 0.0f)
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

                const Vec3f halfVector =
                    SafeNormalize(viewDirection + lightDirection, hit->Normal);

                const float nDotH =
                    std::max(Dot(hit->Normal, halfVector), 0.0f);

                const float hDotV =
                    std::max(Dot(halfVector, viewDirection), 0.0f);

                const Color3f specular =
                    FresnelSchlick(hDotV, f0) *
                    (DistributionGGX(nDotH, roughness) *
                     GeometrySchlickGGX(nDotV, roughness) *
                     GeometrySchlickGGX(nDotL, roughness) /
                        std::max(4.0f * nDotV * nDotL, 1.0e-5f));

                const Color3f diffuse =
                    albedo *
                    ((1.0f - metallic) / std::numbers::pi_v<float>);

                const float attenuation =
                    light.Intensity /
                    std::max(lightDistance * lightDistance, scene.Settings.MinimumLightDistanceSquared);

                color +=
                    (diffuse + specular) *
                    light.Color *
                    (nDotL * attenuation / static_cast<float>(samples));
            }
        }

        return color;
    }
}
