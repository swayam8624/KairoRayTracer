#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

import Kairo.Foundation.RayTracer;

using namespace kairo::foundation::raytracer;

namespace
{
    void PrintUsage()
    {
        std::cout
            << "Usage: KairoRayTracerPreview <scene.kairo> [--mode whitted|normal|depth|shadow_mask|bvh_heatmap]\n";
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

        std::string modeOverride;
        for (int i = 2; i < argc; ++i)
        {
            const std::string arg =
                argv[i];

            if (arg == "--mode" && i + 1 < argc)
            {
                modeOverride = argv[++i];
            }
            else
            {
                PrintUsage();
                return 1;
            }
        }

        auto renderScene =
            [&]() -> RenderResult
            {
                Scene scene =
                    LoadScene(scenePath);

                if (!modeOverride.empty())
                {
                    scene.Settings.Mode =
                        RenderModeFromString(modeOverride);
                }

                std::cout << "Rendering preview "
                          << scene.Settings.Width << "x" << scene.Settings.Height
                          << " mode=" << ToString(scene.Settings.Mode) << "...\n";

                RenderResult result =
                    Renderer{}.Render(scene);

                std::cout << "Rendered in " << result.Stats.RenderMilliseconds << " ms\n";
                return result;
            };

        RenderResult result =
            renderScene();

        const std::filesystem::path savePath =
            std::filesystem::path("outputs") /
            (modeOverride.empty()
                ? "beauty.ppm"
                : OutputNameForMode(RenderModeFromString(modeOverride)));

        SavePPM(result.Image, savePath);

        std::ostringstream title;
        title << "KairoRayTracer - " << scenePath.filename().string()
              << " - " << result.Image.Width() << "x" << result.Image.Height()
              << " - " << (modeOverride.empty() ? "scene mode" : modeOverride)
              << " - " << result.Stats.RenderMilliseconds << " ms"
              << " | Esc close, S save, R rerender";

        RunPreviewWindow(
            std::move(result.Image),
            PreviewOptions
            {
                title.str(),
                savePath
            },
            [&]() -> Film
            {
                return renderScene().Image;
            });

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "KairoRayTracerPreview failed: " << exception.what() << "\n";
        return 1;
    }
}
