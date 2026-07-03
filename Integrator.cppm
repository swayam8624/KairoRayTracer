module;

#include <string>
#include <stdexcept>

export module Kairo.Foundation.RayTracer.Integrator;

import Kairo.Foundation.RayTracer.Types;

export namespace kairo::foundation::raytracer
{
    // Integrator selection is string-based at the executable/parser boundary and
    // enum-based inside the renderer. That keeps runtime dispatch cheap and
    // catches typos before rendering starts.
    [[nodiscard]]
    inline RenderMode RenderModeFromString(
        const std::string& value)
    {
        if (value == "normal") return RenderMode::Normal;
        if (value == "depth") return RenderMode::Depth;
        if (value == "whitted") return RenderMode::Whitted;
        if (value == "shadow_mask") return RenderMode::ShadowMask;
        if (value == "bvh_heatmap") return RenderMode::BVHHeatmap;
        if (value == "albedo") return RenderMode::Albedo;
        if (value == "primitive_id") return RenderMode::PrimitiveID;
        if (value == "uv") return RenderMode::UV;
        if (value == "barycentric") return RenderMode::Barycentric;
        if (value == "accel_diff") return RenderMode::AccelerationDifference;
        if (value == "pbr") return RenderMode::PBR;
        if (value == "path") return RenderMode::Path;

        throw std::invalid_argument("Unknown render mode: " + value);
    }

    [[nodiscard]]
    inline AccelerationMode AccelerationModeFromString(
        const std::string& value)
    {
        if (value == "brute") return AccelerationMode::BruteForce;
        if (value == "sah") return AccelerationMode::BVHSAH;
        if (value == "morton") return AccelerationMode::BVHMorton;

        throw std::invalid_argument("Unknown acceleration mode: " + value);
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
        case RenderMode::Albedo: return "albedo.ppm";
        case RenderMode::PrimitiveID: return "primitive_id.ppm";
        case RenderMode::UV: return "uv.ppm";
        case RenderMode::Barycentric: return "barycentric.ppm";
        case RenderMode::AccelerationDifference: return "accel_diff.ppm";
        case RenderMode::PBR: return "pbr.ppm";
        case RenderMode::Path: return "path.ppm";
        }

        return "output.ppm";
    }
}
