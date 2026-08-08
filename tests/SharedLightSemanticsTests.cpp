#include <catch2/catch_test_macros.hpp>

import Kairo.Foundation.RayTracer;
import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Ray;

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
