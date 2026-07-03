module;

#include <vector>

export module Kairo.Foundation.RayTracer.Mesh;

import Kairo.Foundation.Geometry.Triangle;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::geometry;

    struct TriangleMesh final
    {
        std::vector<Trianglef> Triangles;
    };
}
