module;

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <utility>

export module Kairo.Foundation.RayTracer.ProgressiveRenderer;

import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Film;
import Kairo.Foundation.RayTracer.Renderer;
import Kairo.Foundation.RayTracer.Scene;
import Kairo.Foundation.RayTracer.Types;

export namespace kairo::foundation::raytracer
{
    //=========================================================
    // Progressive Rendering
    //
    // Progressive accumulation is not a new integrator. It is a scheduling mode:
    // render one deterministic sample pass at a time, average the completed
    // passes, and keep an inspectable intermediate image after every pass in a
    // future preview/editor.
    //=========================================================

    struct ProgressiveRenderResult final
    {
        Film Image;
        RenderStats Stats;
        std::uint32_t CompletedPasses = 0;
    };

    [[nodiscard]]
    inline ProgressiveRenderResult RenderProgressive(
        const Scene& scene,
        std::uint32_t passes)
    {
        if (passes == 0)
        {
            throw std::invalid_argument("Progressive render pass count must be greater than zero.");
        }

        ValidateRenderSettings(scene.Settings);

        Film accumulated(scene.Settings.Width, scene.Settings.Height);
        RenderStats mergedStats;

        for (std::uint32_t pass = 0; pass < passes; ++pass)
        {
            Scene passScene =
                scene;

            passScene.Settings.SamplesPerPixel = 1;
            passScene.Settings.SampleSeed = scene.Settings.SampleSeed + pass;

            const RenderResult passResult =
                Renderer{}.Render(passScene);

            for (std::size_t i = 0; i < accumulated.Pixels().size(); ++i)
            {
                accumulated.Pixels()[i] += passResult.Image.Pixels()[i];
            }

            Accumulate(mergedStats, passResult.Stats);
            mergedStats.RenderMilliseconds += passResult.Stats.RenderMilliseconds;
        }

        for (std::size_t i = 0; i < accumulated.Pixels().size(); ++i)
        {
            accumulated.Pixels()[i] =
                accumulated.Pixels()[i] / static_cast<float>(passes);
        }

        FinalizeDerivedStats(mergedStats);

        return
        {
            std::move(accumulated),
            mergedStats,
            passes
        };
    }
}
