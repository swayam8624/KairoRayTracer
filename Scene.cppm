module;

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module Kairo.Foundation.RayTracer.Scene;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Ray;
import Kairo.Foundation.Geometry.Sphere;
import Kairo.Foundation.Geometry.Triangle;
import Kairo.Foundation.Spatial;
import Kairo.Foundation.RayTracer.Types;
import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Camera;
import Kairo.Foundation.RayTracer.Material;
import Kairo.Foundation.RayTracer.Light;
import Kairo.Foundation.RayTracer.Primitive;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;
    using namespace kairo::foundation::geometry;
    namespace spatial = kairo::foundation::spatial;

    //=========================================================
    // Scene
    //
    // Scene is the bridge between render concepts and acceleration structures:
    // materials/lights/primitives are ray-tracing concepts, while the BVH is a
    // KairoSpatial concept. The scene owns both and translates between them.
    //=========================================================

    class Scene final
    {
    public:
        Camera MainCamera;
        RenderSettings Settings;
        std::vector<Material> Materials;
        std::vector<PointLight> Lights;
        std::vector<AreaLight> AreaLights;
        std::vector<Primitive> Primitives;

        [[nodiscard]]
        const spatial::BVH& Acceleration() const noexcept
        {
            return m_BVH;
        }

        [[nodiscard]]
        bool HasAcceleration() const noexcept
        {
            return m_BVH.IsValid() && !m_BVH.Empty();
        }

        [[nodiscard]]
        std::uint32_t AddMaterial(
            Material material)
        {
            // Materials are stored in a dense vector. Primitives keep integer
            // material indices so variants remain small and serialization later
            // can reference materials by name/index.
            Materials.push_back(std::move(material));
            return static_cast<std::uint32_t>(Materials.size() - 1);
        }

        void AddSphere(
            const Vec3f& center,
            float radius,
            std::uint32_t materialIndex)
        {
            ValidateMaterialIndex(materialIndex);
            if (radius <= 0.0f)
            {
                throw std::invalid_argument("Sphere radius must be positive.");
            }

            Primitives.push_back(
                SpherePrimitive
                {
                    Spheref::FromCenterRadius(center, radius),
                    materialIndex
                });
        }

        void AddTriangle(
            const Vec3f& a,
            const Vec3f& b,
            const Vec3f& c,
            std::uint32_t materialIndex)
        {
            ValidateMaterialIndex(materialIndex);

            TrianglePrimitive primitive
            {
                Trianglef::FromPoints(a, b, c),
                materialIndex
            };

            if (primitive.Triangle.AreaSquared() <= 1.0e-10f)
            {
                throw std::invalid_argument("Triangle primitive is degenerate.");
            }

            Primitives.push_back(primitive);
        }

        void BuildAcceleration()
        {
            // SpatialPrimitive is intentionally simpler than a render primitive:
            // it only needs identity, bounds, and a layer mask. UserID/UserIndex
            // both map back to Primitives[i] in V1.
            std::vector<spatial::SpatialPrimitive> spatialPrimitives;
            spatialPrimitives.reserve(Primitives.size());

            for (std::size_t i = 0; i < Primitives.size(); ++i)
            {
                spatialPrimitives.push_back(
                    spatial::SpatialPrimitive
                    {
                        static_cast<spatial::SpatialID>(i),
                        Bounds(Primitives[i]),
                        1u
                    });
            }

            spatial::BVHBuildSettings settings;
            settings.MaxLeafSize = 4;
            settings.UseSAH = true;
            m_BVH = spatial::BuildBVH(spatialPrimitives, settings);
        }

        [[nodiscard]]
        std::optional<SurfaceHit> Intersect(
            const Rayf& ray,
            RenderStats* stats = nullptr) const
        {
            // BVH traversal only decides which render primitives are worth exact
            // testing. The callback performs true sphere/triangle intersections
            // and returns a SpatialRayHit so KairoSpatial can sort closest hits.
            if (Primitives.empty())
            {
                return std::nullopt;
            }

            if (m_BVH.Empty())
            {
                return BruteForceIntersect(ray);
            }

            const spatial::SpatialRaycastResult result =
                spatial::Raycast(
                    m_BVH,
                    ray,
                    [&](spatial::SpatialIndex primitiveIndex, const spatial::SpatialRay& queryRay)
                        -> std::optional<spatial::SpatialRayHit>
                    {
                        const std::optional<SurfaceHit> hit =
                            raytracer::Intersect(
                                Primitives.at(primitiveIndex),
                                queryRay,
                                primitiveIndex);

                        if (!hit)
                        {
                            return std::nullopt;
                        }

                        return spatial::SpatialRayHit
                        {
                            static_cast<spatial::SpatialID>(primitiveIndex),
                            primitiveIndex,
                            hit->Distance,
                            hit->Position,
                            hit->Normal
                        };
                    });

            if (stats)
            {
                stats->BVHVisitedNodes += result.VisitedNodes;
                stats->BVHTestedPrimitives += result.TestedPrimitives;
            }

            if (!result.Hit)
            {
                return std::nullopt;
            }

            const std::uint32_t primitiveIndex =
                result.Closest.PrimitiveIndex;

            // Recompute the exact render hit after BVH traversal has identified
            // the nearest primitive. This preserves renderer-only data such as
            // triangle barycentric UVs that SpatialRayHit does not store.
            return raytracer::Intersect(
                Primitives.at(primitiveIndex),
                ray,
                primitiveIndex);
        }

        [[nodiscard]]
        std::optional<SurfaceHit> BruteForceIntersect(
            const Rayf& ray) const
        {
            // Brute force is kept as a correctness oracle. Tests compare it
            // against BVH traversal so acceleration can change without silently
            // changing visible results.
            std::optional<SurfaceHit> closest;
            float closestDistance =
                std::numeric_limits<float>::infinity();

            for (std::uint32_t i = 0; i < Primitives.size(); ++i)
            {
                const std::optional<SurfaceHit> hit =
                    raytracer::Intersect(Primitives[i], ray, i);

                if (hit && hit->Distance < closestDistance)
                {
                    closest = hit;
                    closestDistance = hit->Distance;
                }
            }

            return closest;
        }

        [[nodiscard]]
        bool Occluded(
            const Rayf& ray,
            float maxDistance,
            RenderStats* stats = nullptr) const
        {
            // Occlusion uses the "any hit" traversal because shadows do not need
            // the nearest blocker. This is the acceleration win that makes hard
            // shadows cheap enough for every light and pixel.
            if (Primitives.empty() || maxDistance <= 0.0f)
            {
                return false;
            }

            if (m_BVH.Empty())
            {
                for (std::uint32_t i = 0; i < Primitives.size(); ++i)
                {
                    const std::optional<SurfaceHit> hit =
                        raytracer::Intersect(Primitives[i], ray, i);

                    if (hit && hit->Distance > 0.0f && hit->Distance < maxDistance)
                    {
                        return true;
                    }
                }

                return false;
            }

            return spatial::RaycastAny(
                m_BVH,
                ray,
                [&](spatial::SpatialIndex primitiveIndex, const spatial::SpatialRay& queryRay)
                {
                    const std::optional<SurfaceHit> hit =
                        raytracer::Intersect(Primitives.at(primitiveIndex), queryRay, primitiveIndex);

                    if (stats)
                    {
                        ++stats->BVHTestedPrimitives;
                    }

                    return hit && hit->Distance > 0.0f && hit->Distance < maxDistance;
                },
                maxDistance);
        }

    private:
        spatial::BVH m_BVH;

        void ValidateMaterialIndex(
            std::uint32_t materialIndex) const
        {
            if (materialIndex >= Materials.size())
            {
                throw std::out_of_range("Primitive references a material index that does not exist.");
            }
        }
    };
}
