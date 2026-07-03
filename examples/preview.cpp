#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

import Kairo.Foundation.RayTracer;
import Kairo.Foundation.RayTracer.PreviewWindow;

using namespace kairo::foundation::raytracer;

// Preview executable:
// - Renders the scene exactly like the CLI.
// - Saves the PPM artifact.
// - Opens a GLFW window to display the completed Film.
//
// This gives a "real window" without mixing in ImGui/editor architecture.
namespace
{
    void PrintUsage()
    {
        std::cout
            << "Usage: KairoRayTracerPreview <scene.kairo> [--mode whitted|pbr|normal|depth|shadow_mask|bvh_heatmap|albedo|primitive_id|uv|barycentric|accel_diff] [--width px --height px] [--samples n]\n";
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
        std::uint32_t widthOverride = 0;
        std::uint32_t heightOverride = 0;
        std::uint32_t samplesOverride = 0;
        for (int i = 2; i < argc; ++i)
        {
            const std::string arg =
                argv[i];

            if (arg == "--mode" && i + 1 < argc)
            {
                modeOverride = argv[++i];
            }
            else if (arg == "--width" && i + 1 < argc)
            {
                widthOverride = static_cast<std::uint32_t>(std::stoul(argv[++i]));
            }
            else if (arg == "--height" && i + 1 < argc)
            {
                heightOverride = static_cast<std::uint32_t>(std::stoul(argv[++i]));
            }
            else if (arg == "--samples" && i + 1 < argc)
            {
                samplesOverride = static_cast<std::uint32_t>(std::stoul(argv[++i]));
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
                // Re-load the scene for each render so pressing R picks up file
                // edits made in a text editor.
                Scene scene =
                    LoadScene(scenePath);

                if (!modeOverride.empty())
                {
                    scene.Settings.Mode =
                        RenderModeFromString(modeOverride);
                }

                if (widthOverride != 0)
                {
                    scene.Settings.Width = widthOverride;
                }

                if (heightOverride != 0)
                {
                    scene.Settings.Height = heightOverride;
                }

                if (samplesOverride != 0)
                {
                    scene.Settings.SamplesPerPixel = samplesOverride;
                }

                scene.MainCamera.AspectRatio =
                    static_cast<float>(scene.Settings.Width) /
                    static_cast<float>(scene.Settings.Height);

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

        SaveImage(result.Image, savePath);

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
                // The preview layer only asks for a new Film. It does not know
                // how scenes, BVHs, or integrators work.
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
