#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

import Kairo.Foundation.RayTracer;
import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Ray;
import Kairo.Foundation.Geometry.Sphere;

using namespace kairo::foundation::raytracer;
using namespace kairo::foundation::math;
using namespace kairo::foundation::geometry;

// These tests are intentionally small and deterministic. They are not image
// golden tests yet; instead they validate the contracts that make images sane:
// parsing, camera rays, primitive hits, BVH equivalence, shadows, rendering, and
// PPM output.
namespace
{
    const char* MinimalSceneText()
    {
        return R"(
resolution 64 48
samples 1
background 0.01 0.02 0.03
integrator whitted
camera 0 1.2 5 0 0.8 0 0 1 0 45
material red lambert 0.8 0.1 0.1
material mirror mirror 0.9 0.9 0.95
material floor lambert 0.5 0.5 0.5
light point 0 4 2 1 1 1 8
triangle -3 0 -3 3 0 -3 3 0 3 floor
triangle -3 0 -3 3 0 3 -3 0 3 floor
sphere 0 1 0 1 red
)";
    }

    [[nodiscard]]
    std::filesystem::path SceneRoot()
    {
        return std::filesystem::path(__FILE__).parent_path().parent_path() / "scenes";
    }

    [[nodiscard]]
    bool IsFiniteColor(
        const Color3f& color)
    {
        return
            std::isfinite(color.r) &&
            std::isfinite(color.g) &&
            std::isfinite(color.b);
    }

    void RequireFiniteImage(
        const Film& film)
    {
        for (const Color3f& pixel : film.Pixels())
        {
            REQUIRE(IsFiniteColor(pixel));
        }
    }

    [[nodiscard]]
    bool HasNonBlackPixel(
        const Film& film)
    {
        for (const Color3f& pixel : film.Pixels())
        {
            if (IsNonBlack(pixel))
            {
                return true;
            }
        }

        return false;
    }

    void RequireImagesNear(
        const Film& a,
        const Film& b,
        float tolerance)
    {
        REQUIRE(a.Width() == b.Width());
        REQUIRE(a.Height() == b.Height());
        REQUIRE(a.Pixels().size() == b.Pixels().size());

        for (std::size_t i = 0; i < a.Pixels().size(); ++i)
        {
            REQUIRE(a.Pixels()[i].r == Catch::Approx(b.Pixels()[i].r).margin(tolerance));
            REQUIRE(a.Pixels()[i].g == Catch::Approx(b.Pixels()[i].g).margin(tolerance));
            REQUIRE(a.Pixels()[i].b == Catch::Approx(b.Pixels()[i].b).margin(tolerance));
        }
    }

    void ConfigureTinyRender(
        Scene& scene,
        RenderMode mode,
        std::uint32_t width = 24,
        std::uint32_t height = 16,
        std::uint32_t samples = 1,
        std::uint32_t threads = 1)
    {
        scene.Settings.Mode = mode;
        scene.Settings.Width = width;
        scene.Settings.Height = height;
        scene.Settings.SamplesPerPixel = samples;
        scene.Settings.ThreadCount = threads;
        scene.Settings.TileSize = 8;
        scene.MainCamera.AspectRatio =
            static_cast<float>(scene.Settings.Width) /
            static_cast<float>(scene.Settings.Height);
    }

    [[nodiscard]]
    std::uint32_t ReadU32BE(
        const unsigned char* bytes)
    {
        return
            (static_cast<std::uint32_t>(bytes[0]) << 24u) |
            (static_cast<std::uint32_t>(bytes[1]) << 16u) |
            (static_cast<std::uint32_t>(bytes[2]) << 8u) |
            static_cast<std::uint32_t>(bytes[3]);
    }
}

TEST_CASE("Scene parser loads valid kairo scene", "[RayTracer][Parser]")
{
    // A valid scene must produce all runtime systems: settings, camera,
    // materials, lights, primitives, and acceleration.
    const Scene scene =
        ParseSceneText(MinimalSceneText());

    REQUIRE(scene.Settings.Width == 64);
    REQUIRE(scene.Settings.Height == 48);
    REQUIRE(scene.Materials.size() == 3);
    REQUIRE(scene.Lights.size() == 1);
    REQUIRE(scene.Primitives.size() == 3);
    REQUIRE(scene.HasAcceleration());
}

TEST_CASE("Scene parser reports line and column for invalid input", "[RayTracer][Parser]")
{
    try
    {
        (void)ParseSceneText(
            "resolution 64 48\n"
            "camera 0 0 5 0 0 0 0 1 0 45\n"
            "material red lambert 1 0 0\n"
            "sphere 0 0 0 1 missing\n");

        FAIL("Expected parse failure.");
    }
    catch (const SceneParseException& exception)
    {
        REQUIRE(exception.Error().Line == 4);
        REQUIRE(exception.Error().Column > 0);
        REQUIRE(std::string(exception.what()).find("unknown material") != std::string::npos);
    }
}

TEST_CASE("Camera center ray points toward target", "[RayTracer][Camera]")
{
    const Camera camera =
        Camera::LookAt(
            Vec3f{ 0.0f, 0.0f, 5.0f },
            Vec3f::Zero(),
            Vec3f::Up(),
            45.0f,
            1.0f);

    const Rayf ray =
        camera.GenerateRay(0.5f, 0.5f);

    REQUIRE(Dot(ray.Direction, Vec3f{ 0.0f, 0.0f, -1.0f }) == Catch::Approx(1.0f).margin(1.0e-5f));
    REQUIRE(ray.IsDirectionNormalized());
}

TEST_CASE("Primitive intersections return material and distance", "[RayTracer][Primitive]")
{
    const Primitive primitive =
        SpherePrimitive
        {
            Spheref::FromCenterRadius(Vec3f::Zero(), 1.0f),
            7
        };

    const auto hit =
        Intersect(
            primitive,
            Rayf::FromOriginDirection(Vec3f{ 0.0f, 0.0f, 5.0f }, Vec3f{ 0.0f, 0.0f, -1.0f }),
            3);

    REQUIRE(hit.has_value());
    REQUIRE(hit->Distance == Catch::Approx(4.0f).margin(1.0e-5f));
    REQUIRE(hit->PrimitiveIndex == 3);
    REQUIRE(hit->MaterialIndex == 7);
}

TEST_CASE("Scene BVH intersection matches brute force", "[RayTracer][Scene]")
{
    // The brute-force path is the oracle. If this fails, BVH traversal is
    // returning a different visible surface than the simple O(n) loop.
    const Scene scene =
        ParseSceneText(MinimalSceneText());

    const Rayf ray =
        scene.MainCamera.GenerateRay(0.5f, 0.5f);

    const auto bvhHit =
        scene.Intersect(ray);

    const auto bruteHit =
        scene.BruteForceIntersect(ray);

    REQUIRE(bvhHit.has_value());
    REQUIRE(bruteHit.has_value());
    REQUIRE(bvhHit->PrimitiveIndex == bruteHit->PrimitiveIndex);
    REQUIRE(bvhHit->Distance == Catch::Approx(bruteHit->Distance).margin(1.0e-5f));
    REQUIRE(bvhHit->MaterialIndex == bruteHit->MaterialIndex);
    REQUIRE(bvhHit->UV.x == Catch::Approx(bruteHit->UV.x).margin(1.0e-5f));
    REQUIRE(bvhHit->UV.y == Catch::Approx(bruteHit->UV.y).margin(1.0e-5f));
}

TEST_CASE("BVH preserves triangle barycentric UV data", "[RayTracer][Scene]")
{
    const Scene scene =
        ParseSceneText(R"(
resolution 32 32
samples 1
background 0 0 0
integrator normal
camera 0 0 3 0 0 0 0 1 0 45
material tri lambert 1 1 1
triangle -1 -1 0  1 -1 0  0 1 0 tri
)");

    const Rayf ray =
        Rayf::FromOriginDirection(
            Vec3f{ 0.0f, 0.0f, 3.0f },
            Vec3f{ 0.0f, 0.0f, -1.0f });

    const auto bvhHit =
        scene.Intersect(ray);

    const auto bruteHit =
        scene.BruteForceIntersect(ray);

    REQUIRE(bvhHit.has_value());
    REQUIRE(bruteHit.has_value());
    REQUIRE(bvhHit->UV.x == Catch::Approx(bruteHit->UV.x).margin(1.0e-5f));
    REQUIRE(bvhHit->UV.y == Catch::Approx(bruteHit->UV.y).margin(1.0e-5f));
    REQUIRE(bvhHit->UV.x > 0.0f);
    REQUIRE(bvhHit->UV.y > 0.0f);
}

TEST_CASE("Stratified sampler does not duplicate center rays", "[RayTracer][Sampler]")
{
    const PixelSample a =
        StratifiedPixelSample(4, 7, 0, 4);

    const PixelSample b =
        StratifiedPixelSample(4, 7, 1, 4);

    REQUIRE(a.dx >= 0.0f);
    REQUIRE(a.dx < 1.0f);
    REQUIRE(a.dy >= 0.0f);
    REQUIRE(a.dy < 1.0f);
    REQUIRE((a.dx != b.dx || a.dy != b.dy));
}

TEST_CASE("Scene shadow occlusion distinguishes blocker and empty direction", "[RayTracer][Scene]")
{
    const Scene scene =
        ParseSceneText(MinimalSceneText());

    REQUIRE(scene.Occluded(
        Rayf::FromOriginDirection(Vec3f{ 0.0f, 1.0f, 4.0f }, Vec3f{ 0.0f, 0.0f, -1.0f }),
        10.0f));

    REQUIRE_FALSE(scene.Occluded(
        Rayf::FromOriginDirection(Vec3f{ 0.0f, 4.0f, 4.0f }, Vec3f{ 0.0f, 1.0f, 0.0f }),
        10.0f));
}

TEST_CASE("Renderer produces non-black Whitted image", "[RayTracer][Renderer]")
{
    // This is a smoke test for the whole render path. It does not prove beauty,
    // but it catches black-frame regressions from camera, parser, lighting, or
    // intersection bugs.
    Scene scene =
        ParseSceneText(MinimalSceneText());

    scene.Settings.Mode = RenderMode::Whitted;
    scene.Settings.Width = 32;
    scene.Settings.Height = 24;
    scene.MainCamera.AspectRatio = static_cast<float>(scene.Settings.Width) / static_cast<float>(scene.Settings.Height);

    const RenderResult result =
        Renderer{}.Render(scene);

    RequireFiniteImage(result.Image);
    REQUIRE(HasNonBlackPixel(result.Image));
    REQUIRE(result.Stats.PrimaryRays == 32u * 24u);
}

TEST_CASE("PBR image renders finite pixels", "[RayTracer][Renderer]")
{
    Scene scene =
        LoadScene(SceneRoot() / "pbr_showcase.kairo");

    ConfigureTinyRender(scene, RenderMode::PBR, 24, 16, 1, 1);

    const RenderResult result =
        Renderer{}.Render(scene);

    RequireFiniteImage(result.Image);
    REQUIRE(HasNonBlackPixel(result.Image));
}

TEST_CASE("Path mode renders deterministic finite image", "[RayTracer][Renderer]")
{
    Scene scene =
        LoadScene(SceneRoot() / "path_showcase.kairo");

    ConfigureTinyRender(scene, RenderMode::Path, 20, 14, 4, 2);

    const RenderResult first =
        Renderer{}.Render(scene);

    const RenderResult second =
        Renderer{}.Render(scene);

    RequireFiniteImage(first.Image);
    RequireFiniteImage(second.Image);
    REQUIRE(HasNonBlackPixel(first.Image));
    RequireImagesNear(first.Image, second.Image, 0.0f);
}

TEST_CASE("OBJ mesh scene loads and renders", "[RayTracer][Renderer]")
{
    Scene scene =
        LoadScene(SceneRoot() / "mesh_showcase.kairo");

    ConfigureTinyRender(scene, RenderMode::Whitted, 24, 16, 1, 1);

    const RenderResult result =
        Renderer{}.Render(scene);

    RequireFiniteImage(result.Image);
    REQUIRE(HasNonBlackPixel(result.Image));
}

TEST_CASE("Area light soft-shadow scene renders", "[RayTracer][Renderer]")
{
    Scene scene =
        LoadScene(SceneRoot() / "area_light_soft_shadow.kairo");

    ConfigureTinyRender(scene, RenderMode::Whitted, 24, 16, 1, 1);

    const RenderResult result =
        Renderer{}.Render(scene);

    RequireFiniteImage(result.Image);
    REQUIRE(HasNonBlackPixel(result.Image));
    REQUIRE(result.Stats.ShadowRays > 0);
}

TEST_CASE("Threaded render equals single-threaded render", "[RayTracer][Renderer]")
{
    Scene singleThread =
        ParseSceneText(MinimalSceneText());

    ConfigureTinyRender(singleThread, RenderMode::Whitted, 24, 16, 4, 1);

    Scene multiThread =
        singleThread;
    multiThread.Settings.ThreadCount = 4;

    const RenderResult single =
        Renderer{}.Render(singleThread);

    const RenderResult threaded =
        Renderer{}.Render(multiThread);

    RequireImagesNear(single.Image, threaded.Image, 0.0f);
}

TEST_CASE("Fixed scene renders deterministic images by mode", "[RayTracer][Renderer]")
{
    const RenderMode modes[] =
    {
        RenderMode::Whitted,
        RenderMode::PBR,
        RenderMode::Path
    };

    for (RenderMode mode : modes)
    {
        Scene scene =
            ParseSceneText(MinimalSceneText());

        ConfigureTinyRender(scene, mode, 18, 12, mode == RenderMode::Path ? 4u : 1u, 2);

        const RenderResult first =
            Renderer{}.Render(scene);

        const RenderResult second =
            Renderer{}.Render(scene);

        RequireImagesNear(first.Image, second.Image, 0.0f);
        RequireFiniteImage(first.Image);
    }
}

TEST_CASE("SamplesPerPixel 1 4 and 16 render finite images", "[RayTracer][Renderer]")
{
    const std::uint32_t sampleCounts[] =
    {
        1,
        4,
        16
    };

    for (std::uint32_t sampleCount : sampleCounts)
    {
        Scene scene =
            ParseSceneText(MinimalSceneText());

        ConfigureTinyRender(scene, RenderMode::Whitted, 16, 12, sampleCount, 2);

        const RenderResult result =
            Renderer{}.Render(scene);

        RequireFiniteImage(result.Image);
        REQUIRE(result.Stats.PrimaryRays == 16u * 12u * sampleCount);
    }
}

TEST_CASE("Renderer rejects invalid render settings", "[RayTracer][Renderer]")
{
    auto requireInvalid =
        [](auto mutate)
        {
            Scene scene =
                ParseSceneText(MinimalSceneText());

            mutate(scene.Settings);

            REQUIRE_THROWS_AS(Renderer{}.Render(scene), std::invalid_argument);
        };

    requireInvalid([](RenderSettings& settings) { settings.Width = 0; });
    requireInvalid([](RenderSettings& settings) { settings.Height = 0; });
    requireInvalid([](RenderSettings& settings) { settings.SamplesPerPixel = 0; });
    requireInvalid([](RenderSettings& settings) { settings.TileSize = 0; });
    requireInvalid([](RenderSettings& settings) { settings.DepthNear = 10.0f; settings.DepthFar = 1.0f; });
    requireInvalid([](RenderSettings& settings) { settings.Width = 32769; });
    requireInvalid([](RenderSettings& settings) { settings.Height = 32769; });
    requireInvalid([](RenderSettings& settings) { settings.SamplesPerPixel = 4097; });
    requireInvalid([](RenderSettings& settings) { settings.TileSize = 4097; });
    requireInvalid([](RenderSettings& settings) { settings.MaxDepth = 65; });
    requireInvalid([](RenderSettings& settings) { settings.ThreadCount = 1025; });
}

TEST_CASE("PPM writer emits valid header", "[RayTracer][ImageIO]")
{
    Film film(2, 1);
    film.SetPixel(0, 0, Color3f::White());
    film.SetPixel(1, 0, Color3f::Black());

    const std::filesystem::path outputPath =
        std::filesystem::temp_directory_path() / "kairo_raytracer_test.ppm";

    SavePPM(film, outputPath);

    std::ifstream in(outputPath);
    std::string magic;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t maxValue = 0;

    in >> magic >> width >> height >> maxValue;

    REQUIRE(magic == "P3");
    REQUIRE(width == 2);
    REQUIRE(height == 1);
    REQUIRE(maxValue == 255);
}

TEST_CASE("PNG writer emits valid signature", "[RayTracer][ImageIO]")
{
    Film film(2, 1);
    film.SetPixel(0, 0, Color3f::White());
    film.SetPixel(1, 0, Color3f::Black());

    const std::filesystem::path outputPath =
        std::filesystem::temp_directory_path() / "kairo_raytracer_test.png";

    SavePNG(film, outputPath);

    std::ifstream in(outputPath, std::ios::binary);
    unsigned char header[24] = {};
    in.read(reinterpret_cast<char*>(header), 24);

    REQUIRE(header[0] == 0x89);
    REQUIRE(header[1] == 'P');
    REQUIRE(header[2] == 'N');
    REQUIRE(header[3] == 'G');
    REQUIRE(header[12] == 'I');
    REQUIRE(header[13] == 'H');
    REQUIRE(header[14] == 'D');
    REQUIRE(header[15] == 'R');
    REQUIRE(ReadU32BE(header + 16) == 2);
    REQUIRE(ReadU32BE(header + 20) == 1);
}

TEST_CASE("Stats writer emits benchmark CSV", "[RayTracer][ImageIO]")
{
    RenderStats stats;
    stats.PrimaryRays = 10;
    stats.ShadowRays = 5;
    stats.ReflectionRays = 1;
    stats.HitCount = 8;
    stats.MissCount = 2;
    stats.BVHVisitedNodes = 30;
    stats.BVHTestedPrimitives = 12;
    stats.MaxRecursionDepthReached = 2;
    stats.RenderMilliseconds = 4.0;
    FinalizeDerivedStats(stats);

    const std::filesystem::path outputPath =
        std::filesystem::temp_directory_path() / "kairo_raytracer_stats.csv";

    SaveStatsCSV(stats, outputPath);

    std::ifstream in(outputPath);
    std::string header;
    std::string row;
    std::getline(in, header);
    std::getline(in, row);

    REQUIRE(header.find("primary_rays") != std::string::npos);
    REQUIRE(header.find("rays_per_second") != std::string::npos);
    REQUIRE(row.find("10,5,1,16") != std::string::npos);
}

TEST_CASE("Renderer supports resized threaded output", "[RayTracer][Renderer]")
{
    Scene scene =
        ParseSceneText(MinimalSceneText());

    scene.Settings.Width = 19;
    scene.Settings.Height = 13;
    scene.Settings.SamplesPerPixel = 4;
    scene.Settings.ThreadCount = 2;
    scene.MainCamera.AspectRatio =
        static_cast<float>(scene.Settings.Width) /
        static_cast<float>(scene.Settings.Height);

    const RenderResult result =
        Renderer{}.Render(scene);

    REQUIRE(result.Image.Width() == 19);
    REQUIRE(result.Image.Height() == 13);
    REQUIRE(result.Stats.PrimaryRays == 19u * 13u * 4u);
    REQUIRE(result.Stats.RaysPerSecond > 0.0);
}

TEST_CASE("Bundled scenes parse and build acceleration", "[RayTracer][Scenes]")
{
    const std::filesystem::path sceneRoot =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "scenes";

    const char* sceneNames[] =
    {
        "cornell.kairo",
        "reflective_spheres.kairo",
        "triangle_floor.kairo",
        "shadow_lab.kairo",
        "mirror_hall.kairo",
        "bvh_stress.kairo",
        "normal_depth_lab.kairo",
        "emissive_showcase.kairo",
        "parser_reference.kairo",
        "glass_refraction.kairo",
        "mesh_showcase.kairo",
        "pbr_showcase.kairo",
        "path_showcase.kairo",
        "area_light_soft_shadow.kairo"
    };

    for (const char* sceneName : sceneNames)
    {
        const Scene scene =
            LoadScene(sceneRoot / sceneName);

        REQUIRE(scene.Settings.Width > 0);
        REQUIRE(scene.Settings.Height > 0);
        REQUIRE(!scene.Materials.empty());
        REQUIRE(!scene.Primitives.empty());
        REQUIRE(scene.HasAcceleration());
    }
}

TEST_CASE("Acceleration difference mode is black when BVH matches brute force", "[RayTracer][Debug]")
{
    Scene scene =
        ParseSceneText(MinimalSceneText());

    scene.Settings.Mode = RenderMode::AccelerationDifference;
    scene.Settings.Width = 16;
    scene.Settings.Height = 12;
    scene.MainCamera.AspectRatio =
        static_cast<float>(scene.Settings.Width) /
        static_cast<float>(scene.Settings.Height);

    const RenderResult result =
        Renderer{}.Render(scene);

    for (const Color3f& pixel : result.Image.Pixels())
    {
        REQUIRE(pixel.r == Catch::Approx(0.0f).margin(1.0e-5f));
        REQUIRE(pixel.g == Catch::Approx(0.0f).margin(1.0e-5f));
        REQUIRE(pixel.b == Catch::Approx(0.0f).margin(1.0e-5f));
    }
}
