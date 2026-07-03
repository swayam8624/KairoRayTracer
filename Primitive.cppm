module;

#include <algorithm>
#include <cstdint>
#include <optional>
#include <variant>

export module Kairo.Foundation.RayTracer.Primitive;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Sphere;
import Kairo.Foundation.Geometry.Triangle;
import Kairo.Foundation.Geometry.AABB;
import Kairo.Foundation.Geometry.Ray;
import Kairo.Foundation.Geometry.Intersection;
import Kairo.Foundation.RayTracer.Types;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;
    using namespace kairo::foundation::geometry;

    //=========================================================
    // Primitive Model
    //
    // The renderer uses std::variant instead of virtual inheritance. That keeps
    // ownership explicit, avoids heap allocation per primitive, and lets the BVH
    // store primitive indices that map directly back to this vector.
    //=========================================================

    struct SpherePrimitive final
    {
        Spheref Sphere;
        std::uint32_t MaterialIndex = 0;
    };

    struct TrianglePrimitive final
    {
        Trianglef Triangle;
        std::uint32_t MaterialIndex = 0;
    };

    using Primitive =
        std::variant<SpherePrimitive, TrianglePrimitive>;

    [[nodiscard]]
    inline AABBf Bounds(
        const SpherePrimitive& primitive) noexcept
    {
        // A sphere's world-space AABB is center +/- radius on each axis.
        const Vec3f extent
        {
            primitive.Sphere.Radius,
            primitive.Sphere.Radius,
            primitive.Sphere.Radius
        };

        return AABBf::FromMinMax(
            primitive.Sphere.Center - extent,
            primitive.Sphere.Center + extent);
    }

    [[nodiscard]]
    inline AABBf Bounds(
        const TrianglePrimitive& primitive) noexcept
    {
        // Triangle bounds are the component-wise min/max of the three vertices.
        // A tiny padding prevents exactly flat triangles from producing zero
        // thickness boxes that are awkward for broadphase ray/AABB tests.
        const Vec3f min
        {
            std::min({ primitive.Triangle.A.x, primitive.Triangle.B.x, primitive.Triangle.C.x }),
            std::min({ primitive.Triangle.A.y, primitive.Triangle.B.y, primitive.Triangle.C.y }),
            std::min({ primitive.Triangle.A.z, primitive.Triangle.B.z, primitive.Triangle.C.z })
        };

        const Vec3f max
        {
            std::max({ primitive.Triangle.A.x, primitive.Triangle.B.x, primitive.Triangle.C.x }),
            std::max({ primitive.Triangle.A.y, primitive.Triangle.B.y, primitive.Triangle.C.y }),
            std::max({ primitive.Triangle.A.z, primitive.Triangle.B.z, primitive.Triangle.C.z })
        };

        constexpr float padding = 1.0e-4f;
        return AABBf::FromMinMax(
            min - Vec3f{ padding, padding, padding },
            max + Vec3f{ padding, padding, padding });
    }

    [[nodiscard]]
    inline AABBf Bounds(
        const Primitive& primitive)
    {
        return std::visit(
            [](const auto& typedPrimitive)
            {
                return Bounds(typedPrimitive);
            },
            primitive);
    }

    [[nodiscard]]
    inline std::uint32_t MaterialIndex(
        const Primitive& primitive)
    {
        return std::visit(
            [](const auto& typedPrimitive)
            {
                return typedPrimitive.MaterialIndex;
            },
            primitive);
    }

    [[nodiscard]]
    inline std::optional<SurfaceHit> Intersect(
        const SpherePrimitive& primitive,
        const Rayf& ray,
        std::uint32_t primitiveIndex)
    {
        // KairoGeometry computes the geometric ray/sphere hit. The ray tracer
        // wraps that result with primitive and material identity for shading.
        const std::optional<RayHitf> hit =
            RaySphere(ray, primitive.Sphere);

        if (!hit)
        {
            return std::nullopt;
        }

        return SurfaceHit
        {
            true,
            hit->Distance,
            hit->Point,
            hit->Normal,
            Vec2f::Zero(),
            primitiveIndex,
            primitive.MaterialIndex
        };
    }

    [[nodiscard]]
    inline std::optional<SurfaceHit> Intersect(
        const TrianglePrimitive& primitive,
        const Rayf& ray,
        std::uint32_t primitiveIndex)
    {
        // Moller-Trumbore intersection lives in KairoGeometry. UVs are stored as
        // barycentric B/C weights for future texture coordinates.
        const std::optional<RayHitf> hit =
            RayTriangle(ray, primitive.Triangle);

        if (!hit)
        {
            return std::nullopt;
        }

        const Vec3f barycentric =
            primitive.Triangle.Barycentric(hit->Point);

        return SurfaceHit
        {
            true,
            hit->Distance,
            hit->Point,
            hit->Normal,
            Vec2f{ barycentric.y, barycentric.z },
            primitiveIndex,
            primitive.MaterialIndex
        };
    }

    [[nodiscard]]
    inline std::optional<SurfaceHit> Intersect(
        const Primitive& primitive,
        const Rayf& ray,
        std::uint32_t primitiveIndex)
    {
        return std::visit(
            [&](const auto& typedPrimitive) -> std::optional<SurfaceHit>
            {
                return Intersect(typedPrimitive, ray, primitiveIndex);
            },
            primitive);
    }
}
