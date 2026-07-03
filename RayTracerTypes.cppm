module;

#include <cstdint>
#include <limits>
#include <string>

export module Kairo.Foundation.RayTracer.Types;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.RayTracer.Color;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;

    //=========================================================
    // Render Modes
    //
    // V1 deliberately exposes several "debug integrators". A beauty render can
    // hide math bugs, while normal/depth/shadow/BVH images isolate one system at
    // a time. This is how offline renderers are usually debugged.
    //=========================================================

    enum class RenderMode : std::uint8_t
    {
        Normal,
        Depth,
        Whitted,
        ShadowMask,
        BVHHeatmap
    };

    // RenderSettings is the small contract between scene parsing, command-line
    // overrides, and the renderer. Keep it plain-data so tests and examples can
    // construct scenes without hidden ownership or services.
    struct RenderSettings final
    {
        std::uint32_t Width = 800;
        std::uint32_t Height = 600;
        std::uint32_t SamplesPerPixel = 1;
        std::uint32_t MaxDepth = 4;
        std::uint32_t TileSize = 16;
        std::uint32_t ThreadCount = 0;
        float DepthNear = 0.1f;
        float DepthFar = 25.0f;
        float RayBiasAbsolute = 1.0e-4f;
        float RayBiasRelative = 1.0e-4f;
        float MinimumLightDistanceSquared = 0.35f;
        RenderMode Mode = RenderMode::Whitted;
        Color3f Background = { 0.02f, 0.03f, 0.05f };
    };

    // RenderStats is intentionally approximate and cheap. It is not a profiler;
    // it is a sanity view that answers: did rays fire, did the BVH traverse, and
    // is a mode unexpectedly doing too much work?
    struct RenderStats final
    {
        std::uint64_t PrimaryRays = 0;
        std::uint64_t ShadowRays = 0;
        std::uint64_t ReflectionRays = 0;
        std::uint64_t HitCount = 0;
        std::uint64_t MissCount = 0;
        std::uint64_t BVHVisitedNodes = 0;
        std::uint64_t BVHTestedPrimitives = 0;
        std::uint32_t MaxRecursionDepthReached = 0;
        double RenderMilliseconds = 0.0;
        double RaysPerSecond = 0.0;
        double PrimitiveTestsPerRay = 0.0;
        double NodesVisitedPerRay = 0.0;
    };

    [[nodiscard]]
    inline std::uint64_t TotalRays(
        const RenderStats& stats) noexcept
    {
        return stats.PrimaryRays + stats.ShadowRays + stats.ReflectionRays;
    }

    inline void Accumulate(
        RenderStats& target,
        const RenderStats& source) noexcept
    {
        target.PrimaryRays += source.PrimaryRays;
        target.ShadowRays += source.ShadowRays;
        target.ReflectionRays += source.ReflectionRays;
        target.HitCount += source.HitCount;
        target.MissCount += source.MissCount;
        target.BVHVisitedNodes += source.BVHVisitedNodes;
        target.BVHTestedPrimitives += source.BVHTestedPrimitives;
        target.MaxRecursionDepthReached =
            target.MaxRecursionDepthReached > source.MaxRecursionDepthReached
                ? target.MaxRecursionDepthReached
                : source.MaxRecursionDepthReached;
    }

    inline void FinalizeDerivedStats(
        RenderStats& stats) noexcept
    {
        const double totalRays =
            static_cast<double>(TotalRays(stats));

        const double seconds =
            stats.RenderMilliseconds / 1000.0;

        stats.RaysPerSecond =
            seconds > 0.0 ? totalRays / seconds : 0.0;

        stats.PrimitiveTestsPerRay =
            totalRays > 0.0
                ? static_cast<double>(stats.BVHTestedPrimitives) / totalRays
                : 0.0;

        stats.NodesVisitedPerRay =
            totalRays > 0.0
                ? static_cast<double>(stats.BVHVisitedNodes) / totalRays
                : 0.0;
    }

    [[nodiscard]]
    inline float RayBiasForHit(
        const RenderSettings& settings,
        float hitDistance) noexcept
    {
        const float relative =
            settings.RayBiasRelative * (hitDistance > 0.0f ? hitDistance : 1.0f);

        return settings.RayBiasAbsolute > relative
            ? settings.RayBiasAbsolute
            : relative;
    }

    // SurfaceHit is richer than SpatialRayHit because shading needs material,
    // normal, UV, and render primitive identity. Spatial remains a broadphase
    // acceleration layer; this struct is the renderer's shading record.
    struct SurfaceHit final
    {
        bool Hit = false;
        float Distance = std::numeric_limits<float>::infinity();
        Vec3f Position = Vec3f::Zero();
        Vec3f Normal = Vec3f::Up();
        Vec2f UV = Vec2f::Zero();
        std::uint32_t PrimitiveIndex = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t MaterialIndex = std::numeric_limits<std::uint32_t>::max();
    };

    // SceneParseError is separate from std::runtime_error so tests and tools can
    // inspect exact source locations without parsing error strings.
    struct SceneParseError final
    {
        std::uint32_t Line = 0;
        std::uint32_t Column = 0;
        std::string Message;
    };

    [[nodiscard]]
    inline const char* ToString(
        RenderMode mode) noexcept
    {
        switch (mode)
        {
        case RenderMode::Normal: return "normal";
        case RenderMode::Depth: return "depth";
        case RenderMode::Whitted: return "whitted";
        case RenderMode::ShadowMask: return "shadow_mask";
        case RenderMode::BVHHeatmap: return "bvh_heatmap";
        }

        return "unknown";
    }
}
