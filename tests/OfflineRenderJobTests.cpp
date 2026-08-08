#include <catch2/catch_test_macros.hpp>

import Kairo.Foundation.RayTracer.OfflineRenderJob;
import Kairo.Foundation.RayTracer.Scene;

TEST_CASE("Offline render capability diagnostics accept environment-only scenes")
{
    using namespace kairo::foundation::raytracer;
    Scene scene;
    scene.Settings.Width = 8u;
    scene.Settings.Height = 8u;
    scene.Settings.SamplesPerPixel = 1u;
    const auto report = InspectOfflineRenderScene(scene);
    CHECK(report.Supported);
    REQUIRE_FALSE(report.Diagnostics.empty());
}

TEST_CASE("Offline render jobs publish progressive results")
{
    using namespace kairo::foundation::raytracer;
    Scene scene;
    scene.Settings.Width = 4u;
    scene.Settings.Height = 4u;
    scene.Settings.SamplesPerPixel = 1u;
    OfflineRenderJob job(std::move(scene), 2u);
    job.Start();
    job.Wait();
    const auto progress = job.Progress();
    CHECK(progress.State == OfflineRenderJobState::Completed);
    CHECK(progress.CompletedPasses == 2u);
    CHECK(progress.Fraction == 1.0);
    const auto image = job.Snapshot();
    REQUIRE(image.has_value());
    CHECK(image->Width() == 4u);
    CHECK(image->Height() == 4u);
}
