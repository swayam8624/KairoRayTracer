module;

#include <string>
#include <stdexcept>

export module Kairo.Foundation.RayTracer.Integrator;

import Kairo.Foundation.RayTracer.Types;

export namespace kairo::foundation::raytracer
{
    [[nodiscard]]
    inline RenderMode RenderModeFromString(
        const std::string& value)
    {
        if (value == "normal") return RenderMode::Normal;
        if (value == "depth") return RenderMode::Depth;
        if (value == "whitted") return RenderMode::Whitted;
        if (value == "shadow_mask") return RenderMode::ShadowMask;
        if (value == "bvh_heatmap") return RenderMode::BVHHeatmap;

        throw std::invalid_argument("Unknown render mode: " + value);
    }

    [[nodiscard]]
    inline const char* OutputNameForMode(
        RenderMode mode) noexcept
    {
        switch (mode)
        {
        case RenderMode::Normal: return "normal.ppm";
        case RenderMode::Depth: return "depth.ppm";
        case RenderMode::Whitted: return "beauty.ppm";
        case RenderMode::ShadowMask: return "shadow_mask.ppm";
        case RenderMode::BVHHeatmap: return "bvh_heatmap.ppm";
        }

        return "output.ppm";
    }
}
