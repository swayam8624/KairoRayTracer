module;

#include <filesystem>
#include <fstream>
#include <stdexcept>

export module Kairo.Foundation.RayTracer.ImageIO;

import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Film;

export namespace kairo::foundation::raytracer
{
    //=========================================================
    // ImageIO
    //
    // PPM is used first because it is completely transparent: a tiny text
    // header plus RGB numbers. That makes output bugs easy to inspect before
    // adding PNG/HDR libraries later.
    //=========================================================

    /// Input: film and destination path.
    /// Output: ASCII PPM image on disk.
    /// Task: provide a dependency-free V1 image format for renderer output.
    inline void SavePPM(
        const Film& film,
        const std::filesystem::path& path)
    {
        if (film.Width() == 0 || film.Height() == 0)
        {
            throw std::invalid_argument("Cannot save an empty film.");
        }

        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream out(path);
        if (!out)
        {
            throw std::runtime_error("Failed to open PPM output path: " + path.string());
        }

        out << "P3\n"
            << film.Width() << " " << film.Height() << "\n"
            << "255\n";

        for (std::uint32_t y = 0; y < film.Height(); ++y)
        {
            for (std::uint32_t x = 0; x < film.Width(); ++x)
            {
                const Color3f color =
                    Saturate(film.GetPixel(x, y));

                out << static_cast<int>(LinearChannelToByte(color.r)) << ' '
                    << static_cast<int>(LinearChannelToByte(color.g)) << ' '
                    << static_cast<int>(LinearChannelToByte(color.b)) << '\n';
            }
        }
    }
}
