module;

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

export module Kairo.Foundation.RayTracer.ImageIO;

import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Film;
import Kairo.Foundation.RayTracer.Types;

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
                    Saturate(ToneMapReinhard(film.GetPixel(x, y)));

                out << static_cast<int>(LinearChannelToByte(color.r)) << ' '
                    << static_cast<int>(LinearChannelToByte(color.g)) << ' '
                    << static_cast<int>(LinearChannelToByte(color.b)) << '\n';
            }
        }
    }

    namespace image_detail
    {
        inline void WriteU32BE(
            std::vector<std::uint8_t>& bytes,
            std::uint32_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
            bytes.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
            bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
            bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
        }

        [[nodiscard]]
        inline std::uint32_t CRC32(
            const std::uint8_t* data,
            std::size_t size) noexcept
        {
            std::uint32_t crc = 0xffffffffu;
            for (std::size_t i = 0; i < size; ++i)
            {
                crc ^= data[i];
                for (int bit = 0; bit < 8; ++bit)
                {
                    crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
                }
            }

            return crc ^ 0xffffffffu;
        }

        [[nodiscard]]
        inline std::uint32_t Adler32(
            const std::vector<std::uint8_t>& data) noexcept
        {
            constexpr std::uint32_t modulus = 65521u;
            std::uint32_t a = 1u;
            std::uint32_t b = 0u;

            for (std::uint8_t byte : data)
            {
                a = (a + byte) % modulus;
                b = (b + a) % modulus;
            }

            return (b << 16u) | a;
        }

        inline void WriteChunk(
            std::vector<std::uint8_t>& png,
            const std::array<char, 4>& type,
            const std::vector<std::uint8_t>& payload)
        {
            WriteU32BE(png, static_cast<std::uint32_t>(payload.size()));

            const std::size_t typeOffset =
                png.size();

            for (char c : type)
            {
                png.push_back(static_cast<std::uint8_t>(c));
            }

            png.insert(png.end(), payload.begin(), payload.end());

            const std::uint32_t crc =
                CRC32(png.data() + typeOffset, 4u + payload.size());

            WriteU32BE(png, crc);
        }

        [[nodiscard]]
        inline std::vector<std::uint8_t> ZlibStore(
            const std::vector<std::uint8_t>& raw)
        {
            std::vector<std::uint8_t> zlib;
            zlib.push_back(0x78u);
            zlib.push_back(0x01u);

            std::size_t offset = 0;
            while (offset < raw.size())
            {
                const std::uint16_t blockSize =
                    static_cast<std::uint16_t>(
                        std::min<std::size_t>(65535u, raw.size() - offset));

                const bool finalBlock =
                    offset + blockSize >= raw.size();

                zlib.push_back(finalBlock ? 0x01u : 0x00u);
                zlib.push_back(static_cast<std::uint8_t>(blockSize & 0xffu));
                zlib.push_back(static_cast<std::uint8_t>((blockSize >> 8u) & 0xffu));

                const std::uint16_t inverted =
                    static_cast<std::uint16_t>(~blockSize);

                zlib.push_back(static_cast<std::uint8_t>(inverted & 0xffu));
                zlib.push_back(static_cast<std::uint8_t>((inverted >> 8u) & 0xffu));

                zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset), raw.begin() + static_cast<std::ptrdiff_t>(offset + blockSize));
                offset += blockSize;
            }

            WriteU32BE(zlib, Adler32(raw));
            return zlib;
        }
    }

    inline void SavePNG(
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

        std::vector<std::uint8_t> raw;
        raw.reserve(static_cast<std::size_t>(film.Height()) * (1u + static_cast<std::size_t>(film.Width()) * 3u));

        for (std::uint32_t y = 0; y < film.Height(); ++y)
        {
            raw.push_back(0u);
            for (std::uint32_t x = 0; x < film.Width(); ++x)
            {
                const Color3f color =
                    Saturate(ToneMapReinhard(film.GetPixel(x, y)));

                raw.push_back(LinearChannelToByte(color.r));
                raw.push_back(LinearChannelToByte(color.g));
                raw.push_back(LinearChannelToByte(color.b));
            }
        }

        std::vector<std::uint8_t> png
        {
            0x89u, 'P', 'N', 'G', '\r', '\n', 0x1au, '\n'
        };

        std::vector<std::uint8_t> ihdr;
        image_detail::WriteU32BE(ihdr, film.Width());
        image_detail::WriteU32BE(ihdr, film.Height());
        ihdr.push_back(8u);
        ihdr.push_back(2u);
        ihdr.push_back(0u);
        ihdr.push_back(0u);
        ihdr.push_back(0u);

        image_detail::WriteChunk(png, { 'I', 'H', 'D', 'R' }, ihdr);
        image_detail::WriteChunk(png, { 'I', 'D', 'A', 'T' }, image_detail::ZlibStore(raw));
        image_detail::WriteChunk(png, { 'I', 'E', 'N', 'D' }, {});

        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            throw std::runtime_error("Failed to open PNG output path: " + path.string());
        }

        out.write(
            reinterpret_cast<const char*>(png.data()),
            static_cast<std::streamsize>(png.size()));
    }

    inline void SaveImage(
        const Film& film,
        const std::filesystem::path& path)
    {
        const std::string extension =
            path.extension().string();

        if (extension == ".png")
        {
            SavePNG(film, path);
            return;
        }

        SavePPM(film, path);
    }

    /// Input: render statistics and destination CSV path.
    /// Output: one-header, one-row CSV file on disk.
    /// Task: make performance comparisons reproducible across resolutions,
    /// samples, modes, and future acceleration experiments.
    inline void SaveStatsCSV(
        const RenderStats& stats,
        const std::filesystem::path& path)
    {
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream out(path);
        if (!out)
        {
            throw std::runtime_error("Failed to open stats CSV path: " + path.string());
        }

        out
            << "primary_rays,shadow_rays,reflection_rays,total_rays,hits,misses,"
            << "bvh_nodes_visited,bvh_primitives_tested,max_recursion_depth,"
            << "render_ms,rays_per_second,primitive_tests_per_ray,nodes_visited_per_ray\n";

        out
            << stats.PrimaryRays << ','
            << stats.ShadowRays << ','
            << stats.ReflectionRays << ','
            << TotalRays(stats) << ','
            << stats.HitCount << ','
            << stats.MissCount << ','
            << stats.BVHVisitedNodes << ','
            << stats.BVHTestedPrimitives << ','
            << stats.MaxRecursionDepthReached << ','
            << stats.RenderMilliseconds << ','
            << stats.RaysPerSecond << ','
            << stats.PrimitiveTestsPerRay << ','
            << stats.NodesVisitedPerRay << '\n';
    }
}
