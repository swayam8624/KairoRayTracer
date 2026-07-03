#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>

import Kairo.Foundation.RayTracer;
import Kairo.Foundation.RayTracer.PreviewWindow;
import Kairo.Foundation.Math.Vector;

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
            << "Usage: KairoRayTracerPreview <scene.kairo> [--mode whitted|pbr|path|normal|depth|shadow_mask|bvh_heatmap|albedo|primitive_id|uv|barycentric|accel_diff] [--width px --height px] [--samples n]\n";
    }

    std::uint32_t ParseU32(
        const std::string& text,
        const std::string& name)
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

        if (value == 0)
        {
            throw std::invalid_argument(name + " must be greater than zero.");
        }

        return value;
    }

    struct CameraOrbitState final
    {
        float YawOffsetRadians = 0.0f;
        float PitchOffsetRadians = 0.0f;
        float DistanceScale = 1.0f;
    };

    void ApplyOrbitCamera(
        Scene& scene,
        const CameraOrbitState& orbit)
    {
        if (orbit.YawOffsetRadians == 0.0f &&
            orbit.PitchOffsetRadians == 0.0f &&
            orbit.DistanceScale == 1.0f)
        {
            return;
        }

        constexpr float baseFocusDistance = 5.0f;
        const float orbitDistance =
            baseFocusDistance * orbit.DistanceScale;

        const Vec3f baseForward =
            scene.MainCamera.Forward;

        const Vec3f focus =
            scene.MainCamera.Position + baseForward * baseFocusDistance;

        const float baseYaw =
            std::atan2(baseForward.z, baseForward.x);

        const float basePitch =
            std::asin(std::clamp(baseForward.y, -1.0f, 1.0f));

        const float yaw =
            baseYaw + orbit.YawOffsetRadians;

        const float pitch =
            std::clamp(
                basePitch + orbit.PitchOffsetRadians,
                -1.45f,
                1.45f);

        const float cosPitch =
            std::cos(pitch);

        const Vec3f orbitForward =
        {
            cosPitch * std::cos(yaw),
            std::sin(pitch),
            cosPitch * std::sin(yaw)
        };

        scene.MainCamera =
            Camera::LookAt(
                focus - orbitForward * orbitDistance,
                focus,
                Vec3f::Up(),
                scene.MainCamera.VerticalFOVDegrees,
                scene.MainCamera.AspectRatio);
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
        CameraOrbitState orbit;
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
                widthOverride = ParseU32(argv[++i], "width");
            }
            else if (arg == "--height" && i + 1 < argc)
            {
                heightOverride = ParseU32(argv[++i], "height");
            }
            else if (arg == "--samples" && i + 1 < argc)
            {
                samplesOverride = ParseU32(argv[++i], "samples");
            }
            else
            {
                PrintUsage();
                return 1;
            }
        }

        auto renderScene =
            [&](const CameraOrbitState& cameraOrbit) -> RenderResult
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

                ApplyOrbitCamera(scene, cameraOrbit);

                std::cout << "Rendering preview "
                          << scene.Settings.Width << "x" << scene.Settings.Height
                          << " mode=" << ToString(scene.Settings.Mode) << "...\n";

                RenderResult result =
                    Renderer{}.Render(scene);

                std::cout << "Rendered in " << result.Stats.RenderMilliseconds << " ms\n";
                return result;
            };

        RenderResult result =
            renderScene(orbit);

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
              << " | Esc close, S save, R rerender, arrows orbit, Q/E zoom, Home reset";

        RunPreviewWindow(
            std::move(result.Image),
            PreviewOptions
            {
                title.str(),
                savePath
            },
            [&](PreviewRenderRequest request) -> Film
            {
                // The preview layer only asks for a new Film. It does not know
                // how scenes, BVHs, or integrators work.
                constexpr float orbitStep =
                    10.0f * std::numbers::pi_v<float> / 180.0f;

                switch (request)
                {
                case PreviewRenderRequest::Reload:
                    break;
                case PreviewRenderRequest::OrbitLeft:
                    orbit.YawOffsetRadians -= orbitStep;
                    break;
                case PreviewRenderRequest::OrbitRight:
                    orbit.YawOffsetRadians += orbitStep;
                    break;
                case PreviewRenderRequest::OrbitUp:
                    orbit.PitchOffsetRadians += orbitStep;
                    break;
                case PreviewRenderRequest::OrbitDown:
                    orbit.PitchOffsetRadians -= orbitStep;
                    break;
                case PreviewRenderRequest::ZoomIn:
                    orbit.DistanceScale = std::max(0.25f, orbit.DistanceScale * 0.85f);
                    break;
                case PreviewRenderRequest::ZoomOut:
                    orbit.DistanceScale = std::min(4.0f, orbit.DistanceScale * 1.15f);
                    break;
                case PreviewRenderRequest::ResetOrbit:
                    orbit = CameraOrbitState{};
                    break;
                }

                return renderScene(orbit).Image;
            });

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "KairoRayTracerPreview failed: " << exception.what() << "\n";
        return 1;
    }
}
