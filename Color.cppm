module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

export module Kairo.Foundation.RayTracer.Color;

export namespace kairo::foundation::raytracer
{
    //=========================================================
    // Color3f
    //
    // Ray tracers should do lighting in linear color space. Linear means
    // "twice the number is twice the light energy", which makes addition,
    // multiplication by albedo, and light attenuation physically meaningful.
    // Monitors and image viewers usually expect gamma-encoded values, so the
    // conversion to bytes happens only at the final output boundary.
    //=========================================================

    /// Linear RGB color used internally by the renderer.
    ///
    /// Convention:
    /// - Values are linear, not sRGB.
    /// - Values may exceed [0, 1] while shading and are clamped only for output.
    struct Color3f final
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;

        [[nodiscard]]
        static constexpr Color3f Black() noexcept { return {}; }

        [[nodiscard]]
        static constexpr Color3f White() noexcept { return { 1.0f, 1.0f, 1.0f }; }

        /// Input: another linear RGB color.
        /// Output: channel-wise sum.
        /// Task: accumulate independent light contributions into one pixel.
        [[nodiscard]]
        constexpr Color3f operator+(
            const Color3f& rhs) const noexcept
        {
            return { r + rhs.r, g + rhs.g, b + rhs.b };
        }

        /// Input: another linear RGB color.
        /// Output: channel-wise difference.
        /// Task: support simple color math in tests/debug calculations.
        [[nodiscard]]
        constexpr Color3f operator-(
            const Color3f& rhs) const noexcept
        {
            return { r - rhs.r, g - rhs.g, b - rhs.b };
        }

        /// Input: another linear RGB color.
        /// Output: channel-wise product.
        /// Task: model albedo filtering, where red surfaces keep red light and absorb other channels.
        [[nodiscard]]
        constexpr Color3f operator*(
            const Color3f& rhs) const noexcept
        {
            return { r * rhs.r, g * rhs.g, b * rhs.b };
        }

        /// Input: scalar multiplier.
        /// Output: color scaled uniformly.
        /// Task: apply light intensity, Lambert terms, averaging, and debug exposure.
        [[nodiscard]]
        constexpr Color3f operator*(
            float scalar) const noexcept
        {
            return { r * scalar, g * scalar, b * scalar };
        }

        /// Input: scalar divisor.
        /// Output: color divided uniformly.
        /// Task: average multiple samples per pixel while guarding division by zero.
        [[nodiscard]]
        constexpr Color3f operator/(
            float scalar) const
        {
            if (scalar == 0.0f)
            {
                throw std::invalid_argument("Color division by zero.");
            }

            return { r / scalar, g / scalar, b / scalar };
        }

        /// Input: color to add.
        /// Output: reference to this color after accumulation.
        /// Task: keep the renderer loop simple while summing samples and lights.
        constexpr Color3f& operator+=(
            const Color3f& rhs) noexcept
        {
            r += rhs.r;
            g += rhs.g;
            b += rhs.b;
            return *this;
        }
    };

    /// Input: scalar and color.
    /// Output: uniformly scaled color.
    /// Task: allow both `color * scalar` and `scalar * color` notation.
    [[nodiscard]]
    constexpr Color3f operator*(
        float scalar,
        const Color3f& color) noexcept
    {
        return color * scalar;
    }

    /// Input: linear color.
    /// Output: color with each channel clamped to [0, 1].
    /// Task: bound color before gamma conversion or debug-image display.
    [[nodiscard]]
    inline Color3f Saturate(
        const Color3f& color) noexcept
    {
        return
        {
            std::clamp(color.r, 0.0f, 1.0f),
            std::clamp(color.g, 0.0f, 1.0f),
            std::clamp(color.b, 0.0f, 1.0f)
        };
    }

    /// Input: linear channel value in arbitrary range.
    /// Output: byte encoded using gamma 1/2.2 after clamping.
    /// Task: convert renderer output into a portable PPM byte channel.
    [[nodiscard]]
    inline std::uint8_t LinearChannelToByte(
        float value) noexcept
    {
        const float clamped =
            std::clamp(value, 0.0f, 1.0f);

        const float gamma =
            std::pow(clamped, 1.0f / 2.2f);

        return static_cast<std::uint8_t>(
            std::clamp(std::lround(gamma * 255.0f), 0l, 255l));
    }

    [[nodiscard]]
    inline bool IsNonBlack(
        const Color3f& color) noexcept
    {
        return color.r > 0.0f || color.g > 0.0f || color.b > 0.0f;
    }
}
