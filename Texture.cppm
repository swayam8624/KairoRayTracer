module;

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module Kairo.Foundation.RayTracer.Texture;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.RayTracer.Color;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;

    //=========================================================
    // Texture
    //
    // Textures are sampled in linear RGB. The V1 loader accepts plain PPM/P3
    // files because that format is transparent, easy to generate in tests, and
    // keeps this renderer independent from stb/libpng while the shading model is
    // still evolving.
    //=========================================================

    enum class TextureFilter : std::uint8_t
    {
        Nearest,
        Bilinear
    };

    struct Texture2D final
    {
        std::string Name;
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        TextureFilter Filter = TextureFilter::Bilinear;
        std::vector<Color3f> Pixels;

        [[nodiscard]]
        bool Empty() const noexcept
        {
            return Width == 0 || Height == 0 || Pixels.empty();
        }
    };

    namespace texture_detail
    {
        [[nodiscard]]
        inline float Wrap01(
            float value) noexcept
        {
            const float wrapped =
                value - std::floor(value);

            return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
        }

        [[nodiscard]]
        inline Color3f Lerp(
            const Color3f& a,
            const Color3f& b,
            float t) noexcept
        {
            return a * (1.0f - t) + b * t;
        }

        [[nodiscard]]
        inline const Color3f& Texel(
            const Texture2D& texture,
            std::uint32_t x,
            std::uint32_t y)
        {
            return texture.Pixels.at(
                static_cast<std::size_t>(y) * texture.Width + x);
        }

        [[nodiscard]]
        inline std::string NextPPMToken(
            std::istream& input)
        {
            std::string token;
            char c = '\0';

            while (input.get(c))
            {
                if (std::isspace(static_cast<unsigned char>(c)))
                {
                    continue;
                }

                if (c == '#')
                {
                    std::string ignored;
                    std::getline(input, ignored);
                    continue;
                }

                token.push_back(c);
                break;
            }

            while (input.get(c))
            {
                if (std::isspace(static_cast<unsigned char>(c)))
                {
                    break;
                }

                if (c == '#')
                {
                    std::string ignored;
                    std::getline(input, ignored);
                    break;
                }

                token.push_back(c);
            }

            return token;
        }

        [[nodiscard]]
        inline std::uint32_t ParsePositiveU32(
            const std::string& token,
            const char* fieldName)
        {
            if (token.empty())
            {
                throw std::runtime_error(std::string("PPM is missing ") + fieldName + ".");
            }

            std::uint64_t value = 0;
            for (char c : token)
            {
                if (!std::isdigit(static_cast<unsigned char>(c)))
                {
                    throw std::runtime_error(std::string("PPM has invalid integer for ") + fieldName + ".");
                }

                value = value * 10u + static_cast<unsigned>(c - '0');
                if (value > std::numeric_limits<std::uint32_t>::max())
                {
                    throw std::runtime_error(std::string("PPM integer is too large for ") + fieldName + ".");
                }
            }

            return static_cast<std::uint32_t>(value);
        }
    }

    /// Input: texture, wrapped UV coordinate, and configured filter mode.
    /// Output: linear RGB sample.
    /// Task: provide reusable nearest/bilinear sampling for materials and
    /// environment maps without duplicating filtering logic inside integrators.
    [[nodiscard]]
    inline Color3f SampleTexture(
        const Texture2D& texture,
        const Vec2f& uv)
    {
        if (texture.Empty())
        {
            return Color3f::White();
        }

        const float u =
            texture_detail::Wrap01(uv.x);

        const float v =
            texture_detail::Wrap01(uv.y);

        if (texture.Filter == TextureFilter::Nearest || texture.Width == 1 || texture.Height == 1)
        {
            const std::uint32_t x =
                std::min<std::uint32_t>(
                    static_cast<std::uint32_t>(u * static_cast<float>(texture.Width)),
                    texture.Width - 1u);

            const std::uint32_t y =
                std::min<std::uint32_t>(
                    static_cast<std::uint32_t>((1.0f - v) * static_cast<float>(texture.Height)),
                    texture.Height - 1u);

            return texture_detail::Texel(texture, x, y);
        }

        const float px =
            u * static_cast<float>(texture.Width) - 0.5f;

        const float py =
            (1.0f - v) * static_cast<float>(texture.Height) - 0.5f;

        const float fx =
            px - std::floor(px);

        const float fy =
            py - std::floor(py);

        const auto wrapX =
            [&](int x) -> std::uint32_t
            {
                const int width =
                    static_cast<int>(texture.Width);

                return static_cast<std::uint32_t>((x % width + width) % width);
            };

        const auto wrapY =
            [&](int y) -> std::uint32_t
            {
                const int height =
                    static_cast<int>(texture.Height);

                return static_cast<std::uint32_t>((y % height + height) % height);
            };

        const int x0 =
            static_cast<int>(std::floor(px));

        const int y0 =
            static_cast<int>(std::floor(py));

        const Color3f c00 =
            texture_detail::Texel(texture, wrapX(x0), wrapY(y0));

        const Color3f c10 =
            texture_detail::Texel(texture, wrapX(x0 + 1), wrapY(y0));

        const Color3f c01 =
            texture_detail::Texel(texture, wrapX(x0), wrapY(y0 + 1));

        const Color3f c11 =
            texture_detail::Texel(texture, wrapX(x0 + 1), wrapY(y0 + 1));

        return texture_detail::Lerp(
            texture_detail::Lerp(c00, c10, fx),
            texture_detail::Lerp(c01, c11, fx),
            fy);
    }

    /// Input: PPM/P3 image path and texture metadata.
    /// Output: Texture2D with linear RGB pixels.
    /// Task: load small test/showcase textures without external dependencies.
    [[nodiscard]]
    inline Texture2D LoadPPMTexture(
        const std::filesystem::path& path,
        std::string name,
        TextureFilter filter = TextureFilter::Bilinear)
    {
        std::ifstream input(path);
        if (!input)
        {
            throw std::runtime_error("Failed to open texture: " + path.string());
        }

        const std::string magic =
            texture_detail::NextPPMToken(input);

        if (magic != "P3")
        {
            throw std::runtime_error("Texture loader currently supports ASCII PPM/P3 only: " + path.string());
        }

        Texture2D texture;
        texture.Name = std::move(name);
        texture.Width = texture_detail::ParsePositiveU32(texture_detail::NextPPMToken(input), "width");
        texture.Height = texture_detail::ParsePositiveU32(texture_detail::NextPPMToken(input), "height");
        texture.Filter = filter;

        const std::uint32_t maxValue =
            texture_detail::ParsePositiveU32(texture_detail::NextPPMToken(input), "max value");

        if (texture.Width == 0 || texture.Height == 0 || maxValue == 0)
        {
            throw std::runtime_error("Texture dimensions and max value must be positive: " + path.string());
        }

        texture.Pixels.reserve(
            static_cast<std::size_t>(texture.Width) * texture.Height);

        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(texture.Width) * texture.Height; ++i)
        {
            const std::uint32_t r =
                texture_detail::ParsePositiveU32(texture_detail::NextPPMToken(input), "red channel");

            const std::uint32_t g =
                texture_detail::ParsePositiveU32(texture_detail::NextPPMToken(input), "green channel");

            const std::uint32_t b =
                texture_detail::ParsePositiveU32(texture_detail::NextPPMToken(input), "blue channel");

            if (r > maxValue || g > maxValue || b > maxValue)
            {
                throw std::runtime_error("Texture channel exceeds declared PPM max value: " + path.string());
            }

            const float invMax =
                1.0f / static_cast<float>(maxValue);

            texture.Pixels.push_back(
                Color3f
                {
                    static_cast<float>(r) * invMax,
                    static_cast<float>(g) * invMax,
                    static_cast<float>(b) * invMax
                });
        }

        return texture;
    }
}
