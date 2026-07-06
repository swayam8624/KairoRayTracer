#include <filesystem>
#include <iostream>
#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

import Kairo.Foundation.RayTracer;

using namespace kairo::foundation::raytracer;

// CLI executable:
// - Loads a .kairo scene file.
// - Optionally overrides the render mode/output path.
// - Renders once and writes a PPM image.
//
// This is the most reproducible way to test the ray tracer because every output
// is a file artifact that can be compared, opened, or shared.
namespace
{
    void PrintUsage()
    {
        std::cout
            << "Usage: KairoRayTracerCLI <scene.kairo> [--mode whitted|pbr|path|normal|depth|shadow_mask|bvh_heatmap|albedo|primitive_id|uv|barycentric|accel_diff] [--accel brute|sah|morton] [--output path] [--stats path.csv] [--width px --height px] [--samples n] [--passes n] [--threads n]\n";
    }

    std::uint32_t ParseU32(
        const std::string& text,
        const std::string& name,
        bool allowZero = false)
    {
        if (text.empty() || text.front() == '-')
        {
            throw std::invalid_argument(name + " must be a non-negative integer.");
        }

        std::uint32_t value = 0;
        const char* begin = text.data();
        const char* end = text.data() + text.size();
        const std::from_chars_result result =
            std::from_chars(begin, end, value);

        if (result.ec != std::errc{} || result.ptr != end)
        {
            throw std::invalid_argument(name + " must be a valid unsigned integer.");
        }

        if (!allowZero && value == 0)
        {
            throw std::invalid_argument(name + " must be greater than zero.");
        }

        return value;
    }
}

int main(
    int argc,
    char** argv)
{
    if (argc < 2)
    {
        PrintUsage();
        return 1;
    }

    try
    {
        const std::filesystem::path scenePath =
            argv[1];

        Scene scene =
            LoadScene(scenePath);

        std::filesystem::path outputPath =
            std::filesystem::path("outputs") / OutputNameForMode(scene.Settings.Mode);

        std::filesystem::path statsPath;
        bool rebuildAcceleration = false;
        std::uint32_t progressivePasses = 1;

        for (int i = 2; i < argc; ++i)
        {
            // Argument parsing is intentionally tiny and explicit. This keeps V1
            // dependency-free while still supporting the two overrides needed
            // most during debugging: render mode and output file.
            const std::string arg =
                argv[i];

            if (arg == "--mode" && i + 1 < argc)
            {
                scene.Settings.Mode =
                    RenderModeFromString(argv[++i]);
                outputPath =
                    std::filesystem::path("outputs") / OutputNameForMode(scene.Settings.Mode);
            }
            else if (arg == "--accel" && i + 1 < argc)
            {
                scene.Settings.Acceleration =
                    AccelerationModeFromString(argv[++i]);
                rebuildAcceleration = true;
            }
            else if (arg == "--output" && i + 1 < argc)
            {
                outputPath = argv[++i];
            }
            else if (arg == "--stats" && i + 1 < argc)
            {
                statsPath = argv[++i];
            }
            else if (arg == "--width" && i + 1 < argc)
            {
                scene.Settings.Width =
                    ParseU32(argv[++i], "width");
                scene.MainCamera.AspectRatio =
                    static_cast<float>(scene.Settings.Width) /
                    static_cast<float>(scene.Settings.Height);
            }
            else if (arg == "--height" && i + 1 < argc)
            {
                scene.Settings.Height =
                    ParseU32(argv[++i], "height");
                scene.MainCamera.AspectRatio =
                    static_cast<float>(scene.Settings.Width) /
                    static_cast<float>(scene.Settings.Height);
            }
            else if (arg == "--samples" && i + 1 < argc)
            {
                scene.Settings.SamplesPerPixel =
                    ParseU32(argv[++i], "samples");
            }
            else if (arg == "--passes" && i + 1 < argc)
            {
                progressivePasses =
                    ParseU32(argv[++i], "passes");
            }
            else if (arg == "--threads" && i + 1 < argc)
            {
                scene.Settings.ThreadCount =
                    ParseU32(argv[++i], "threads", true);
            }
            else
            {
                PrintUsage();
                return 1;
            }
        }

        if (rebuildAcceleration)
        {
            scene.BuildAcceleration();
        }

        std::cout << "Rendering "
                  << scene.Settings.Width << "x" << scene.Settings.Height
                  << " mode=" << ToString(scene.Settings.Mode)
                  << " accel=" << ToString(scene.Settings.Acceleration)
                  << " passes=" << progressivePasses
                  << " scene=" << scenePath << "...\n";

        ProgressiveRenderResult progressiveResult;
        if (progressivePasses > 1)
        {
            progressiveResult =
                RenderProgressive(scene, progressivePasses);
        }
        else
        {
            RenderResult singlePass =
                Renderer{}.Render(scene);

            progressiveResult =
                ProgressiveRenderResult
                {
                    std::move(singlePass.Image),
                    singlePass.Stats,
                    1
                };
        }

        // The renderer returns a CPU Film. ImageIO owns the file encoding step,
        // which is why adding PNG later will not change the render loop.
        SaveImage(progressiveResult.Image, outputPath);

        if (!statsPath.empty())
        {
            SaveStatsCSV(progressiveResult.Stats, statsPath);
        }

        std::cout << "Done in " << progressiveResult.Stats.RenderMilliseconds << " ms\n"
                  << "Saved " << outputPath << "\n";

        if (!statsPath.empty())
        {
            std::cout << "Saved stats CSV = " << statsPath << "\n";
        }

        std::cout << "completed passes = " << progressiveResult.CompletedPasses << "\n"
                  << "primary rays = " << progressiveResult.Stats.PrimaryRays << "\n"
                  << "shadow rays = " << progressiveResult.Stats.ShadowRays << "\n"
                  << "reflection rays = " << progressiveResult.Stats.ReflectionRays << "\n"
                  << "hits = " << progressiveResult.Stats.HitCount << "\n"
                  << "misses = " << progressiveResult.Stats.MissCount << "\n"
                  << "max recursion depth = " << progressiveResult.Stats.MaxRecursionDepthReached << "\n"
                  << "bvh nodes visited = " << progressiveResult.Stats.BVHVisitedNodes << "\n"
                  << "bvh primitives tested = " << progressiveResult.Stats.BVHTestedPrimitives << "\n"
                  << "rays/sec = " << progressiveResult.Stats.RaysPerSecond << "\n"
                  << "primitive tests/ray = " << progressiveResult.Stats.PrimitiveTestsPerRay << "\n"
                  << "nodes visited/ray = " << progressiveResult.Stats.NodesVisitedPerRay << "\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "KairoRayTracerCLI failed: " << exception.what() << "\n";
        return 1;
    }
}
