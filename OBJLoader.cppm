module;

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

export module Kairo.Foundation.RayTracer.OBJLoader;

import Kairo.Assets.MeshArtifact;
import Kairo.Assets.OBJImporter;
import Kairo.Assets.DerivedArtifact;
import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Triangle;
import Kairo.Foundation.RayTracer.Mesh;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;
    using namespace kairo::foundation::geometry;

    /// Compatibility adapter for the scene DSL. Source parsing, index
    /// resolution, triangulation, normal generation, and validation belong to
    /// KairoAssets; this module only maps the portable mesh artifact into the
    /// ray tracer's triangle representation and applies the authored instance
    /// scale/translation.
    [[nodiscard]] inline TriangleMesh LoadOBJMesh(const std::filesystem::path& path,
        float scale = 1.0f, const Vec3f& translation = Vec3f::Zero())
    {
        if (!std::isfinite(scale) || scale == 0.0f)
            throw std::invalid_argument("OBJ instance scale must be finite and non-zero.");
        if (!std::isfinite(translation.x) || !std::isfinite(translation.y) || !std::isfinite(translation.z))
            throw std::invalid_argument("OBJ instance translation must be finite.");
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("Failed to open OBJ file: " + path.string());
        const auto end = input.tellg();
        if (end < 0 || static_cast<std::uintmax_t>(end) > kairo::assets::MaximumDerivedArtifactPayloadBytes)
            throw std::length_error("OBJ source exceeds the 1 GiB safety limit: " + path.string());
        std::vector<std::byte> bytes(static_cast<std::size_t>(end));
        input.seekg(0);
        if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
            throw std::runtime_error("Failed to read OBJ file: " + path.string());

        const kairo::assets::MeshArtifactData artifact = kairo::assets::ParseOBJMesh(bytes);
        TriangleMesh mesh;
        mesh.Triangles.reserve(artifact.Indices.size() / 3u);
        const float normalSign = scale < 0.0f ? -1.0f : 1.0f;
        const auto position = [&](std::uint32_t index)
        {
            const auto& value = artifact.Vertices[index].Position;
            return Vec3f{ value[0], value[1], value[2] } * scale + translation;
        };
        const auto normal = [&](std::uint32_t index)
        {
            const auto& value = artifact.Vertices[index].Normal;
            return Vec3f{ value[0], value[1], value[2] } * normalSign;
        };
        const auto uv = [&](std::uint32_t index)
        {
            const auto& value = artifact.Vertices[index].TexCoord;
            return Vec2f{ value[0], value[1] };
        };

        for (std::size_t offset = 0u; offset < artifact.Indices.size(); offset += 3u)
        {
            const std::uint32_t a = artifact.Indices[offset];
            const std::uint32_t b = artifact.Indices[offset + 1u];
            const std::uint32_t c = artifact.Indices[offset + 2u];
            MeshTriangle triangle;
            triangle.Triangle = Trianglef::FromPoints(position(a), position(b), position(c));
            triangle.HasVertexNormals = artifact.HasNormals;
            triangle.HasUVs = artifact.HasTexCoords;
            if (triangle.HasVertexNormals)
            {
                triangle.NormalA = normal(a); triangle.NormalB = normal(b); triangle.NormalC = normal(c);
            }
            if (triangle.HasUVs)
            {
                triangle.UVA = uv(a); triangle.UVB = uv(b); triangle.UVC = uv(c);
            }
            mesh.Triangles.push_back(triangle);
        }
        return mesh;
    }
}
