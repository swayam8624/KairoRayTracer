module;

#include <cstdint>

export module Kairo.Foundation.RayTracer.Light;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.RayTracer.Color;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;

    // PointLight is an infinitesimal emitter. It gives hard-edged shadows
    // because there is no area to sample. That is useful in V1 because shadow
    // correctness is visually obvious.
    struct PointLight final
    {
        Vec3f Position = Vec3f::Zero();
        Color3f Color = Color3f::White();
        float Intensity = 1.0f;
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
