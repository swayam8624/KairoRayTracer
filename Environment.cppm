module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

export module Kairo.Foundation.RayTracer.Environment;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Texture;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;

    //=========================================================
    // Environment Lighting
    //
    // An environment is the radiance returned by a ray that escapes the scene.
    // Keeping it separate from RenderSettings::Background matters because a
    // debug background is just a display color, while an environment participates
    // in reflective/refractive/path-traced lighting.
    //=========================================================

    struct EnvironmentLight final
    {
        bool Enabled = false;
        Color3f Color = Color3f::Black();
        float Intensity = 1.0f;
        std::uint32_t TextureIndex = std::numeric_limits<std::uint32_t>::max();
    };

    [[nodiscard]]
    inline Vec2f DirectionToLatLongUV(
        const Vec3f& direction) noexcept
    {
        constexpr float pi =
            3.14159265358979323846f;

        const Vec3f d =
            direction.Normalized();

        const float u =
            0.5f + std::atan2(d.z, d.x) / (2.0f * pi);

        const float v =
            0.5f - std::asin(std::clamp(d.y, -1.0f, 1.0f)) / pi;

        return { u, v };
    }
}
