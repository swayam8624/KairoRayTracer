#include <filesystem>
#include <iostream>
#include <string>

import Kairo.Foundation.RayTracer;

using namespace kairo::foundation::raytracer;

namespace
{
    void PrintUsage()
    {
        std::cout
            << "Usage: KairoRayTracerCLI <scene.kairo> [--mode whitted|normal|depth|shadow_mask|bvh_heatmap] [--output path]\n";
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

        for (int i = 2; i < argc; ++i)
        {
            const std::string arg =
                argv[i];

            if (arg == "--mode" && i + 1 < argc)
            {
                scene.Settings.Mode =
                    RenderModeFromString(argv[++i]);
                outputPath =
                    std::filesystem::path("outputs") / OutputNameForMode(scene.Settings.Mode);
            }
            else if (arg == "--output" && i + 1 < argc)
            {
                outputPath = argv[++i];
            }
            else
            {
                PrintUsage();
                return 1;
            }
        }

        std::cout << "Rendering "
                  << scene.Settings.Width << "x" << scene.Settings.Height
                  << " mode=" << ToString(scene.Settings.Mode)
                  << " scene=" << scenePath << "...\n";

        const RenderResult result =
            Renderer{}.Render(scene);

        SavePPM(result.Image, outputPath);

        std::cout << "Done in " << result.Stats.RenderMilliseconds << " ms\n"
                  << "Saved " << outputPath << "\n"
                  << "primary rays = " << result.Stats.PrimaryRays << "\n"
                  << "shadow rays = " << result.Stats.ShadowRays << "\n"
                  << "reflection rays = " << result.Stats.ReflectionRays << "\n"
                  << "hits = " << result.Stats.HitCount << "\n"
                  << "bvh nodes visited = " << result.Stats.BVHVisitedNodes << "\n"
                  << "bvh primitives tested = " << result.Stats.BVHTestedPrimitives << "\n";

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "KairoRayTracerCLI failed: " << exception.what() << "\n";
        return 1;
    }
}
