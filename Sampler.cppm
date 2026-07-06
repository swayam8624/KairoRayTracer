module;

#include <algorithm>
#include <cmath>
#include <cstdint>

export module Kairo.Foundation.RayTracer.Sampler;

export namespace kairo::foundation::raytracer
{
    struct PixelSample final
    {
        float dx = 0.5f;
        float dy = 0.5f;
    };

    [[nodiscard]]
    inline float Hash01(
        std::uint32_t value) noexcept
    {
        value ^= value >> 16u;
        value *= 0x7feb352du;
        value ^= value >> 15u;
        value *= 0x846ca68bu;
        value ^= value >> 16u;

        return static_cast<float>(value & 0x00ffffffu) /
            static_cast<float>(0x01000000u);
    }

    [[nodiscard]]
    inline PixelSample StratifiedPixelSample(
        std::uint32_t x,
        std::uint32_t y,
        std::uint32_t sampleIndex,
        std::uint32_t samplesPerPixel,
        std::uint32_t seedOffset = 0) noexcept
    {
        if (samplesPerPixel <= 1u)
        {
            return {};
        }

        const std::uint32_t side =
            static_cast<std::uint32_t>(
                std::ceil(std::sqrt(static_cast<float>(samplesPerPixel))));

        const std::uint32_t sx =
            sampleIndex % side;

        const std::uint32_t sy =
            sampleIndex / side;

        const std::uint32_t seed =
            x * 73856093u ^
            y * 19349663u ^
            sampleIndex * 83492791u ^
            seedOffset * 2654435761u;

        const float jitterX =
            Hash01(seed);

        const float jitterY =
            Hash01(seed ^ 0x9e3779b9u);

        return
        {
            std::clamp((static_cast<float>(sx) + jitterX) / static_cast<float>(side), 0.0f, 0.999999f),
            std::clamp((static_cast<float>(sy) + jitterY) / static_cast<float>(side), 0.0f, 0.999999f)
        };
    }
}
