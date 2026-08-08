module;

#include <cstdint>

export module Kairo.Foundation.RayTracer.Light;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.RayTracer.Color;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;

    // PointLight is an infinitesimal emitter. It gives hard-edged shadows
    // because there is no area to sample. Intensity is authored in candela.
    struct PointLight final
    {
        Vec3f Position = Vec3f::Zero();
        Color3f Color = Color3f::White();
        float Intensity = 1.0f;
    };

    // Direction points in the direction emitted rays travel (light local -Z).
    // Shading therefore uses -Direction as the surface-to-light direction.
    // Intensity is authored as illuminance in lux and does not attenuate with
    // distance because the source is treated as infinitely far away.
    struct DirectionalLight final
    {
        Vec3f Direction = -Vec3f::Up();
        Color3f Color = Color3f::White();
        float Illuminance = 1.0f;
    };

    // Spot lights are point emitters restricted to a cone. Direction is the
    // emitted-ray direction, Range bounds both illumination and shadow rays,
    // and the cone cosines encode a smooth inner-to-outer falloff.
    struct SpotLight final
    {
        Vec3f Position = Vec3f::Zero();
        Vec3f Direction = Vec3f::Forward();
        Color3f Color = Color3f::White();
        float Intensity = 1.0f;
        float Range = 10.0f;
        float InnerConeCosine = 0.93969262f;
        float OuterConeCosine = 0.86602540f;
    };

    struct AreaLight final
    {
        Vec3f Position = Vec3f::Zero();
        Vec3f U = Vec3f::UnitX();
        Vec3f V = Vec3f::Forward();
        Color3f Color = Color3f::White();
        float Intensity = 1.0f;
        std::uint32_t Samples = 4;
    };
}
