// TEACHING MODULE: Scene owns render-facing camera/material/light/primitive
// arrays and a KairoSpatial BVH index over *primitive IDs*. The BVH stores
// simple bounds and IDs, not material/UV attributes. Scene::Intersect uses
// BVH candidates and exact sphere/triangle intersection callbacks; it then
// repeats the nearest exact intersection to recover SurfaceHit UV/barycentric
// metadata for shading. BruteForceIntersect is the comparison oracle.
//
// Primitive IDs are array indices here: changing the array invalidates the
// existing acceleration mapping until BuildAcceleration is called again.
// Ray query results are renderer hits, not PhysicsEngine contact manifolds.
// Renderer and Integrators consume Scene::Intersect/Occluded; ImageIO consumes
// Film pixels rather than a Scene directly.
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
import Kairo.Foundation.RayTracer.Texture;
import Kairo.Foundation.RayTracer.Environment;
import Kairo.Foundation.RayTracer.Mesh;

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
        std::vector<DirectionalLight> DirectionalLights;
        std::vector<SpotLight> SpotLights;
        std::vector<AreaLight> AreaLights;
        std::vector<Primitive> Primitives;
        std::vector<Texture2D> Textures;
        EnvironmentLight Environment;

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
        // Store material by value and return its dense array index.
        // Primitive records keep this index; deleting/reordering
        // Materials without updating primitives invalidates shading.
        std::uint32_t AddMaterial(
            Material material)
        {
            // Materials are stored in a dense vector. Primitives keep integer
            // material indices so variants remain small and serialization later
            // can reference materials by name/index.
            Materials.push_back(std::move(material));
            return static_cast<std::uint32_t>(Materials.size() - 1);
        }

        [[nodiscard]]
        // Empty textures are rejected before becoming renderer data.
        // Environment/material texture references are dense scene
        // indices, not persistent KairoAssets AssetIDs.
        std::uint32_t AddTexture(
            Texture2D texture)
        {
            if (texture.Empty())
            {
                throw std::invalid_argument("Cannot add an empty texture to the scene.");
            }

            Textures.push_back(std::move(texture));
            return static_cast<std::uint32_t>(Textures.size() - 1);
        }

        [[nodiscard]]
        // On a ray miss, choose solid background or enabled
        // environment color/lat-long texture sampling, then apply
        // radiance intensity. There is no geometry surface/UV hit
        // to shade in this path.
        Color3f SampleEnvironment(
            const Vec3f& direction) const
        {
            if (!Environment.Enabled)
            {
                return Settings.Background;
            }

            Color3f radiance =
                Environment.Color;

            if (Environment.TextureIndex < Textures.size())
            {
                radiance =
                    SampleTexture(
                        Textures.at(Environment.TextureIndex),
                        DirectionToLatLongUV(direction));
            }

            return radiance * Environment.Intensity;
        }

        // Reject a nonpositive radius and unknown material before
        // storing a sphere. BuildAcceleration must be rerun before
        // BVH-based queries see changes to the primitive array.
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

        void AddTriangle(
            const MeshTriangle& meshTriangle,
            std::uint32_t materialIndex)
        {
            ValidateMaterialIndex(materialIndex);

            TrianglePrimitive primitive
            {
                meshTriangle.Triangle,
                materialIndex,
                meshTriangle.NormalA,
                meshTriangle.NormalB,
                meshTriangle.NormalC,
                meshTriangle.UVA,
                meshTriangle.UVB,
                meshTriangle.UVC,
                meshTriangle.HasVertexNormals,
                meshTriangle.HasUVs
            };

            if (primitive.Triangle.AreaSquared() <= 1.0e-10f)
            {
                throw std::invalid_argument("Triangle primitive is degenerate.");
            }

            Primitives.push_back(primitive);
        }

        /// TEACHING API: void BuildAcceleration( — Convert source primitives into KairoSpatial bounded records and construct chosen BVH; brute-force path remains an oracle.
        // Rebuild the index from the current primitive vector. A scene
        // geometry edit requires this operation before BVH queries can
        // refer to the correct array indices and shape bounds.
        // Convert each primitive's exact geometry to its AABB and
        // stable-in-this-scene dense primitive index. Configure
        // KairoSpatial BVH (e.g. SAH/Morton) from Settings and build.
        // AABB nodes are only candidate pruning; the shader still
        // uses exact sphere/triangle ray tests for final SurfaceHit.
        // Rebuild a static KairoSpatial BVH from current render primitive *AABBs* and
        // scene-local primitive indexes. SAH/Morton affect traversal cost, not
        // authoritative sphere/triangle geometry. Mutating Primitives afterward
        // invalidates index semantics.
        void BuildAcceleration()
        {
            // Choosing brute force empties the BVH rather than keeping a
            // stale hierarchy. Intersect will test each primitive directly.
            if (Settings.Acceleration == AccelerationMode::BruteForce)
            {
                m_BVH = spatial::BVH{};
                return;
            }

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
            settings.UseSAH = Settings.Acceleration == AccelerationMode::BVHSAH;
            settings.UseMortonOrdering = Settings.Acceleration == AccelerationMode::BVHMorton;
            m_BVH = spatial::BuildBVH(spatialPrimitives, settings);
        }

        [[nodiscard]]
        /// TEACHING API: std::optional<SurfaceHit> Intersect( — Use acceleration candidates then exact primitive hit test to return nearest SurfaceHit and material/UV data.
        // BVH traversal delegates exact geometry hits to the callback.
        // The closest primitive is intersected again after traversal to
        // recover renderer-specific UV, normal and material data.
        // If acceleration is unavailable or brute force selected,
        // test all primitives directly. Otherwise KairoSpatial ray
        // traversal invokes the exact primitive callback and finds
        // the nearest distance; reconstruct full SurfaceHit material
        // and vertex UV/barycentric details for shading.
        // Returned primitive index belongs to the CURRENT scene.
        // Either brute-force all render primitives or traverse BVH with an *exact*
        // primitive ray-hit callback, returning the closest SurfaceHit. Traversal's
        // spatial hit lacks renderer material/UV data, so intersect the winning
        // primitive again to recover shading metadata.
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

            if (Settings.Acceleration == AccelerationMode::BruteForce || m_BVH.Empty())
            {
                return BruteForceIntersect(ray, stats);
            }

            // A BVH node's AABB tests only prune candidate primitives; the callback must
            // perform the actual sphere/triangle intersection. PrimitiveIndex maps to
            // Scene::Primitives while the acceleration is current.
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
        /// TEACHING API: std::optional<SurfaceHit> BruteForceIntersect( — Check every primitive directly; useful to verify BVH hit equivalence on small scenes.
        // O(primitives) exact-hit reference, independent of BVH
        // topology and insertion order. Compare against Intersect
        // in tests to detect missing candidates or bad pruning,
        // not to assert that BVH shading is automatically faster.
        // Test all primitives and keep minimum positive hit distance. This simple
        // algorithm is the correctness oracle for BVH traversal, especially after
        // scene geometry/acceleration changes, despite its O(N) cost per ray.
        std::optional<SurfaceHit> BruteForceIntersect(
            const Rayf& ray,
            RenderStats* stats = nullptr) const
        {
            // Brute force is kept as a correctness oracle. Tests compare it
            // against BVH traversal so acceleration can change without silently
            // changing visible results.
            std::optional<SurfaceHit> closest;
            float closestDistance =
                std::numeric_limits<float>::infinity();

            for (std::uint32_t i = 0; i < Primitives.size(); ++i)
            {
                if (stats)
                {
                    ++stats->BVHTestedPrimitives;
                }

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
        /// TEACHING API: bool Occluded( — Return whether any blocker lies along shadow ray before light distance; do not confuse with nearest-hit shading.
        // Shadows need any blocker before a finite light distance, not
        // the closest material-bearing SurfaceHit. RaycastAny can stop
        // early, making these probes cheaper than full shading queries.
        // Shadow-ray existence query: any surface hit before max
        // light distance blocks direct lighting. It need not find
        // the nearest SurfaceHit or interpolate full shading UVs,
        // which makes it a different contract from Intersect.
        // Shadow queries ask only whether *any* exact intersection lies before
        // maxDistance, not for UV/material at the closest hit. KairoSpatial RaycastAny
        // can terminate early, while brute-force validates existence against geometry.
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

            if (Settings.Acceleration == AccelerationMode::BruteForce || m_BVH.Empty())
            {
                for (std::uint32_t i = 0; i < Primitives.size(); ++i)
                {
                    if (stats)
                    {
                        ++stats->BVHTestedPrimitives;
                    }

                    const std::optional<SurfaceHit> hit =
                        raytracer::Intersect(Primitives[i], ray, i);

                    if (hit && hit->Distance > 0.0f && hit->Distance < maxDistance)
                    {
                        return true;
                    }
                }

                return false;
            }

            // The callback rechecks exact geometry and finite light distance; a traversal
            // bounding-box overlap alone cannot cast a shadow. This query deliberately
            // returns a boolean rather than a full SurfaceHit.
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

        // Prevent a primitive from retaining an out-of-range
        // dense material index, which would otherwise throw later
        // inside the path integrator's hot shading loop.
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
