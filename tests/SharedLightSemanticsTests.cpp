#include <catch2/catch_test_macros.hpp>

import Kairo.Foundation.RayTracer;
import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Ray;
import Kairo.Foundation.Geometry.Sphere;

using namespace kairo::foundation::raytracer;
using namespace kairo::foundation::math;
using namespace kairo::foundation::geometry;

namespace
{
    Scene MakeLitSphereScene()
    {
        Scene scene;
        scene.Settings.Background = Color3f::Black();
        scene.Settings.Mode = RenderMode::PBR;
        scene.Settings.MaxDepth = 4u;
        Material material;
        material.Type = MaterialType::Lambert;
        material.Albedo = { 0.8f, 0.6f, 0.4f };
        material.Roughness = 0.5f;
        const auto materialIndex = scene.AddMaterial(material);
        scene.AddSphere(Sphere{ { 0.0f, 0.0f, 0.0f }, 1.0f }, materialIndex);
        scene.BuildAcceleration();
        return scene;
    }

    Rayf FrontRay()
    {
        return Rayf::FromOriginDirection({ 0.0f, 0.0f, 3.0f }, { 0.0f, 0.0f, -1.0f });
    }
}

TEST_CASE("directional authored light contributes to Whitted and PBR direct shading")
{
    Scene scene = MakeLitSphereScene();
    DirectionalLight sun;
    sun.Direction = { 0.0f, 0.0f, -1.0f };
    sun.Color = { 1.0f, 0.9f, 0.8f };
    sun.Illuminance = 2.0f;
    scene.DirectionalLights.push_back(sun);

    const auto whitted = TraceWhitted(scene, FrontRay(), 0u, nullptr);
    const auto pbr = TracePBRDirect(scene, FrontRay(), nullptr);
    CHECK(IsNonBlack(whitted));
    CHECK(IsNonBlack(pbr));
}

TEST_CASE("spot authored light respects cone direction and range")
{
    Scene scene = MakeLitSphereScene();
    SpotLight spot;
    spot.Position = { 0.0f, 0.0f, 3.0f };
    spot.Direction = { 0.0f, 0.0f, -1.0f };
    spot.Color = Color3f::White();
    spot.Intensity = 20.0f;
    spot.Range = 10.0f;
    spot.InnerConeCosine = 0.98f;
    spot.OuterConeCosine = 0.90f;
    scene.SpotLights.push_back(spot);
    const auto inside = TracePBRDirect(scene, FrontRay(), nullptr);
    CHECK(IsNonBlack(inside));

    scene.SpotLights.front().Direction = { 1.0f, 0.0f, 0.0f };
    const auto outside = TracePBRDirect(scene, FrontRay(), nullptr);
    CHECK(outside.r < inside.r);
    CHECK(outside.g < inside.g);
    CHECK(outside.b < inside.b);
}

TEST_CASE("linear authored fog blends by primary distance")
{
    RenderSettings settings;
    settings.Fog = FogMode::Linear;
    settings.FogColor = { 1.0f, 1.0f, 1.0f };
    settings.FogNear = 2.0f;
    settings.FogFar = 6.0f;

    const Color3f source = Color3f::Black();
    const auto before = ApplyFog(settings, source, 1.0f);
    const auto middle = ApplyFog(settings, source, 4.0f);
    const auto after = ApplyFog(settings, source, 8.0f);

    CHECK(before.r == 0.0f);
    CHECK(middle.r > 0.49f);
    CHECK(middle.r < 0.51f);
    CHECK(after.r == 1.0f);
}

TEST_CASE("exponential authored fog increases monotonically with distance")
{
    RenderSettings settings;
    settings.Fog = FogMode::Exponential;
    settings.FogColor = { 0.2f, 0.4f, 0.8f };
    settings.FogDensity = 0.25f;

    const Color3f source = Color3f::Black();
    const auto nearColor = ApplyFog(settings, source, 1.0f);
    const auto farColor = ApplyFog(settings, source, 8.0f);

    CHECK(farColor.r > nearColor.r);
    CHECK(farColor.g > nearColor.g);
    CHECK(farColor.b > nearColor.b);
}