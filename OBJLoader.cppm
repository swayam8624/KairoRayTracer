module;

#include <filesystem>
#include <fstream>
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
        [[nodiscard]]
        inline int ParseOBJIndex(
            const std::string& token,
            std::size_t vertexCount)
        {
            const std::size_t slash =
                token.find('/');

            const std::string indexText =
                slash == std::string::npos
                    ? token
                    : token.substr(0, slash);

            if (indexText.empty())
            {
                throw std::runtime_error("OBJ face contains an empty vertex index.");
            }

            const int rawIndex =
                std::stoi(indexText);

            if (rawIndex == 0)
            {
                throw std::runtime_error("OBJ indices are 1-based; index 0 is invalid.");
            }

            const int resolved =
                rawIndex > 0
                    ? rawIndex - 1
                    : static_cast<int>(vertexCount) + rawIndex;

            if (resolved < 0 || static_cast<std::size_t>(resolved) >= vertexCount)
            {
                throw std::runtime_error("OBJ face index is out of range.");
            }

            return resolved;
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
            else if (command == "f")
            {
                std::vector<int> indices;
                std::string token;
                while (stream >> token)
                {
                    indices.push_back(obj_detail::ParseOBJIndex(token, vertices.size()));
                }

                if (indices.size() < 3)
                {
                    throw std::runtime_error("OBJ face has fewer than three vertices at line " + std::to_string(line) + ".");
                }

                for (std::size_t i = 1; i + 1 < indices.size(); ++i)
                {
                    mesh.Triangles.push_back(
                        Trianglef::FromPoints(
                            vertices[static_cast<std::size_t>(indices[0])],
                            vertices[static_cast<std::size_t>(indices[i])],
                            vertices[static_cast<std::size_t>(indices[i + 1])]));
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
