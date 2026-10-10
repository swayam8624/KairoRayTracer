#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

import Kairo.Foundation.RayTracer.Film;

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

TEST_CASE("Film resizing rejects invalid geometry without changing the live image")
{
    using namespace kairo::foundation::raytracer;
    Film image(2u, 2u);
    image.SetPixel(0u, 0u, Color3f{ 0.2f, 0.4f, 0.6f });
    const auto original = image.GetPixel(0u, 0u);
    REQUIRE_THROWS_AS(image.Resize(0u, 2u), std::invalid_argument);
    CHECK(image.Width() == 2u);
    CHECK(image.Height() == 2u);
    CHECK(image.Pixels().size() == 4u);
    CHECK(image.GetPixel(0u, 0u) == original);

    image.Resize(3u, 1u);
    CHECK(image.Width() == 3u);
    CHECK(image.Height() == 1u);
    CHECK(image.Pixels().size() == 3u);
    CHECK(image.GetPixel(0u, 0u) == Color3f::Black());
}

TEST_CASE("Offline render snapshot remains available during ongoing passes")
{
    using namespace kairo::foundation::raytracer;
    Scene scene;
    scene.Settings.Width = 16u;
    scene.Settings.Height = 16u;
    scene.Settings.SamplesPerPixel = 1u;
    OfflineRenderJob job(std::move(scene), 12u);
    CHECK_FALSE(job.Snapshot().has_value());
    job.Start();
    for (int attempt = 0; attempt < 1000; ++attempt)
    {
        const auto preview = job.Snapshot();
        if (preview)
        {
            CHECK(preview->Width() == 16u);
            CHECK(preview->Height() == 16u);
            CHECK(preview->Pixels().size() == 256u);
        }
    }
    job.Wait();
    CHECK(job.Progress().State == OfflineRenderJobState::Completed);
    CHECK(job.Progress().CompletedPasses == 12u);
    REQUIRE(job.Snapshot().has_value());
}
