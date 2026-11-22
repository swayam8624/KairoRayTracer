// TEACHING MODULE: Deterministic per-pixel hashing and stratified offsets for anti-aliasing. Sample position affects primary rays; statistical convergence depends on distribution and sample count.
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
    // Mix the supplied 32-bit sample key into a repeatable value in [0,1). This
    // tiny hash is for pixel jitter and sampling decorrelation, not randomness
    // suitable for security or a measured low-discrepancy sequence.
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
    // Divide the unit film pixel into ceil(sqrt(SPP)) strata per axis, select the
    // sampleIndex cell and jitter it with a hash of pixel coordinates, sample
    // index and configured seed. Sampling is independent of which worker claims
    // the tile.
    inline PixelSample StratifiedPixelSample(
        std::uint32_t x,
        std::uint32_t y,
        std::uint32_t sampleIndex,
        std::uint32_t samplesPerPixel,
        std::uint32_t seedOffset = 0) noexcept
    {
        // Single-sample mode places the ray at the exact pixel center (0.5,0.5). This
        // avoids introducing stochastic pixel jitter when one deterministic primary
        // sample is requested.
        if (samplesPerPixel <= 1u)
        {
            return {};
        }

        // For non-square samples per pixel, the final row of the side-by-side grid
        // may remain incomplete. This is finite stratification, not a full balanced
        // n-rooks or Sobol sampling guarantee.
        const std::uint32_t side =
            static_cast<std::uint32_t>(
                std::ceil(std::sqrt(static_cast<float>(samplesPerPixel))));

        const std::uint32_t sx =
            sampleIndex % side;

        const std::uint32_t sy =
            sampleIndex / side;

        // Derive jitter from immutable (x,y,sampleIndex,seedOffset) rather than a
        // shared mutable RNG. Render scheduling/thread count therefore cannot change
        // sampled coordinates for a fixed scene/settings tuple.
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
