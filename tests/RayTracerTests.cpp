#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

    bool anyNonBlack = false;
    for (const Color3f& pixel : result.Image.Pixels())
    {
        anyNonBlack = anyNonBlack || IsNonBlack(pixel);
    }

    REQUIRE(anyNonBlack);
    REQUIRE(result.Stats.PrimaryRays == 32u * 24u);
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
        "parser_reference.kairo"
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
