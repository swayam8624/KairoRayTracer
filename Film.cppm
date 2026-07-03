module;

#include <cstdint>
#include <stdexcept>
#include <vector>

export module Kairo.Foundation.RayTracer.Film;

import Kairo.Foundation.RayTracer.Color;

export namespace kairo::foundation::raytracer
{
    class Film final
    {
    public:
        Film() = default;

        Film(
            std::uint32_t width,
            std::uint32_t height)
        {
            Resize(width, height);
        }

        void Resize(
            std::uint32_t width,
            std::uint32_t height)
        {
            if (width == 0 || height == 0)
            {
                throw std::invalid_argument("Film dimensions must be non-zero.");
            }

            m_Width = width;
            m_Height = height;
            m_Pixels.assign(
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                Color3f::Black());
        }

        [[nodiscard]]
        std::uint32_t Width() const noexcept { return m_Width; }

        [[nodiscard]]
        std::uint32_t Height() const noexcept { return m_Height; }

        [[nodiscard]]
        const std::vector<Color3f>& Pixels() const noexcept { return m_Pixels; }

        [[nodiscard]]
        std::vector<Color3f>& Pixels() noexcept { return m_Pixels; }

        void SetPixel(
            std::uint32_t x,
            std::uint32_t y,
            const Color3f& color)
        {
            m_Pixels.at(IndexOf(x, y)) = color;
        }

        [[nodiscard]]
        const Color3f& GetPixel(
            std::uint32_t x,
            std::uint32_t y) const
        {
            return m_Pixels.at(IndexOf(x, y));
        }

    private:
        std::uint32_t m_Width = 0;
        std::uint32_t m_Height = 0;
        std::vector<Color3f> m_Pixels;

        [[nodiscard]]
        std::size_t IndexOf(
            std::uint32_t x,
            std::uint32_t y) const
        {
            if (x >= m_Width || y >= m_Height)
            {
                throw std::out_of_range("Film pixel coordinate is out of range.");
            }

            return static_cast<std::size_t>(y) * m_Width + x;
        }
    };
}
