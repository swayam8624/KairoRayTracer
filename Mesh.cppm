// TEACHING MAP — Mesh.cppm
// Renderable triangle mesh and intersection/acceleration adapters. OBJLoader supplies
// validated geometry, Scene stores instances, integrators query hit points and normals.
// See READING_ORDER.md for source-to-scene-to-integrator interactions.

module;

#include <vector>

export module Kairo.Foundation.RayTracer.Mesh;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Triangle;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;
    using namespace kairo::foundation::geometry;

    struct MeshTriangle final
    {
        Trianglef Triangle;
        Vec3f NormalA = Vec3f::Zero();
        Vec3f NormalB = Vec3f::Zero();
        Vec3f NormalC = Vec3f::Zero();
        Vec2f UVA = Vec2f::Zero();
        Vec2f UVB = Vec2f::Zero();
        Vec2f UVC = Vec2f::Zero();
        bool HasVertexNormals = false;
        bool HasUVs = false;
    };

    struct TriangleMesh final
    {
        std::vector<MeshTriangle> Triangles;
    };
}
