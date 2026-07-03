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

    enum class RenderMode : std::uint8_t
    {
        Normal,
        Depth,
        Whitted,
        ShadowMask,
        BVHHeatmap
    };

    struct RenderSettings final
    {
        std::uint32_t Width = 800;
        std::uint32_t Height = 600;
        std::uint32_t SamplesPerPixel = 1;
        std::uint32_t MaxDepth = 4;
        float DepthNear = 0.1f;
        float DepthFar = 25.0f;
        float RayBias = 1.0e-3f;
        RenderMode Mode = RenderMode::Whitted;
        Color3f Background = { 0.02f, 0.03f, 0.05f };
    };

    struct RenderStats final
    {
        std::uint64_t PrimaryRays = 0;
        std::uint64_t ShadowRays = 0;
        std::uint64_t ReflectionRays = 0;
        std::uint64_t HitCount = 0;
        std::uint64_t BVHVisitedNodes = 0;
        std::uint64_t BVHTestedPrimitives = 0;
        double RenderMilliseconds = 0.0;
    };

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
