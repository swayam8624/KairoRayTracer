module;

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

export module Kairo.Foundation.RayTracer.OBJLoader;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Triangle;
import Kairo.Foundation.RayTracer.Mesh;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;
    using namespace kairo::foundation::geometry;

    namespace obj_detail
    {
        struct FaceVertex final
        {
            int Vertex = -1;
            int TexCoord = -1;
            int Normal = -1;
        };

        [[nodiscard]]
        inline int ResolveOBJIndex(
            const std::string& indexText,
            std::size_t count,
            const char* label)
        {
            if (indexText.empty())
            {
                return -1;
            }

            const int rawIndex =
                std::stoi(indexText);

            if (rawIndex == 0)
            {
                throw std::runtime_error(std::string("OBJ ") + label + " indices are 1-based; index 0 is invalid.");
            }

            const int resolved =
                rawIndex > 0
                    ? rawIndex - 1
                    : static_cast<int>(count) + rawIndex;

            if (resolved < 0 || static_cast<std::size_t>(resolved) >= count)
            {
                throw std::runtime_error(std::string("OBJ ") + label + " index is out of range.");
            }

            return resolved;
        }

        [[nodiscard]]
        inline FaceVertex ParseFaceVertex(
            const std::string& token,
            std::size_t vertexCount,
            std::size_t texCoordCount,
            std::size_t normalCount)
        {
            const std::size_t firstSlash =
                token.find('/');

            if (firstSlash == std::string::npos)
            {
                return
                {
                    ResolveOBJIndex(token, vertexCount, "vertex"),
                    -1,
                    -1
                };
            }

            const std::size_t secondSlash =
                token.find('/', firstSlash + 1u);

            const std::string vertexText =
                token.substr(0, firstSlash);

            const std::string texCoordText =
                secondSlash == std::string::npos
                    ? token.substr(firstSlash + 1u)
                    : token.substr(firstSlash + 1u, secondSlash - firstSlash - 1u);

            const std::string normalText =
                secondSlash == std::string::npos
                    ? std::string{}
                    : token.substr(secondSlash + 1u);

            return
            {
                ResolveOBJIndex(vertexText, vertexCount, "vertex"),
                ResolveOBJIndex(texCoordText, texCoordCount, "texcoord"),
                ResolveOBJIndex(normalText, normalCount, "normal")
            };
        }

        [[nodiscard]]
        inline MeshTriangle BuildTriangle(
            const std::vector<Vec3f>& vertices,
            const std::vector<Vec2f>& texCoords,
            const std::vector<Vec3f>& normals,
            const FaceVertex& a,
            const FaceVertex& b,
            const FaceVertex& c)
        {
            MeshTriangle triangle;
            triangle.Triangle =
                Trianglef::FromPoints(
                    vertices.at(static_cast<std::size_t>(a.Vertex)),
                    vertices.at(static_cast<std::size_t>(b.Vertex)),
                    vertices.at(static_cast<std::size_t>(c.Vertex)));

            triangle.HasUVs =
                a.TexCoord >= 0 && b.TexCoord >= 0 && c.TexCoord >= 0;

            if (triangle.HasUVs)
            {
                triangle.UVA = texCoords.at(static_cast<std::size_t>(a.TexCoord));
                triangle.UVB = texCoords.at(static_cast<std::size_t>(b.TexCoord));
                triangle.UVC = texCoords.at(static_cast<std::size_t>(c.TexCoord));
            }

            triangle.HasVertexNormals =
                a.Normal >= 0 && b.Normal >= 0 && c.Normal >= 0;

            if (triangle.HasVertexNormals)
            {
                triangle.NormalA = normals.at(static_cast<std::size_t>(a.Normal));
                triangle.NormalB = normals.at(static_cast<std::size_t>(b.Normal));
                triangle.NormalC = normals.at(static_cast<std::size_t>(c.Normal));
            }

            return triangle;
        }
    }

    [[nodiscard]]
    inline TriangleMesh LoadOBJMesh(
        const std::filesystem::path& path,
        float scale = 1.0f,
        const Vec3f& translation = Vec3f::Zero())
    {
        std::ifstream in(path);
        if (!in)
        {
            throw std::runtime_error("Failed to open OBJ file: " + path.string());
        }

        std::vector<Vec3f> vertices;
        std::vector<Vec2f> texCoords;
        std::vector<Vec3f> normals;
        TriangleMesh mesh;

        std::string lineText;
        std::uint32_t line = 0;
        while (std::getline(in, lineText))
        {
            ++line;
            std::istringstream stream(lineText);
            std::string command;
            stream >> command;

            if (command.empty() || command[0] == '#')
            {
                continue;
            }

            if (command == "v")
            {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                if (!(stream >> x >> y >> z))
                {
                    throw std::runtime_error("Invalid OBJ vertex at line " + std::to_string(line) + ".");
                }

                vertices.push_back(Vec3f{ x, y, z } * scale + translation);
            }
            else if (command == "vt")
            {
                float u = 0.0f;
                float v = 0.0f;
                if (!(stream >> u >> v))
                {
                    throw std::runtime_error("Invalid OBJ texcoord at line " + std::to_string(line) + ".");
                }

                texCoords.push_back(Vec2f{ u, v });
            }
            else if (command == "vn")
            {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                if (!(stream >> x >> y >> z))
                {
                    throw std::runtime_error("Invalid OBJ normal at line " + std::to_string(line) + ".");
                }

                normals.push_back(SafeNormalize(Vec3f{ x, y, z }, Vec3f::Up()));
            }
            else if (command == "f")
            {
                std::vector<obj_detail::FaceVertex> indices;
                std::string token;
                while (stream >> token)
                {
                    indices.push_back(
                        obj_detail::ParseFaceVertex(
                            token,
                            vertices.size(),
                            texCoords.size(),
                            normals.size()));
                }

                if (indices.size() < 3)
                {
                    throw std::runtime_error("OBJ face has fewer than three vertices at line " + std::to_string(line) + ".");
                }

                for (std::size_t i = 1; i + 1 < indices.size(); ++i)
                {
                    mesh.Triangles.push_back(
                        obj_detail::BuildTriangle(
                            vertices,
                            texCoords,
                            normals,
                            indices[0],
                            indices[i],
                            indices[i + 1]));
                }
            }
        }

        if (mesh.Triangles.empty())
        {
            throw std::runtime_error("OBJ file contains no triangles: " + path.string());
        }

        return mesh;
    }
}
