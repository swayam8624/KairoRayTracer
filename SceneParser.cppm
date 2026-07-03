module;

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

export module Kairo.Foundation.RayTracer.SceneParser;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Triangle;
import Kairo.Foundation.RayTracer.Types;
import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Camera;
import Kairo.Foundation.RayTracer.Material;
import Kairo.Foundation.RayTracer.Light;
import Kairo.Foundation.RayTracer.Scene;
import Kairo.Foundation.RayTracer.Mesh;
import Kairo.Foundation.RayTracer.OBJLoader;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;

    //=========================================================
    // Scene Parser
    //
    // The .kairo format is deliberately line-oriented for V1. Every command is
    // visible in plain text, parser errors can point to one line/column, and the
    // renderer avoids a JSON dependency while the scene vocabulary is still tiny.
    //=========================================================

    class SceneParseException final : public std::runtime_error
    {
    public:
        explicit SceneParseException(
            SceneParseError error)
            :
            std::runtime_error(
                "scene parse error at line " +
                std::to_string(error.Line) +
                ", column " +
                std::to_string(error.Column) +
                ": " +
                error.Message),
            m_Error(std::move(error))
        {
        }

        [[nodiscard]]
        const SceneParseError& Error() const noexcept
        {
            return m_Error;
        }

    private:
        SceneParseError m_Error;
    };

    namespace parser_detail
    {
        struct Token final
        {
            std::string Text;
            std::uint32_t Column = 1;
        };

        [[nodiscard]]
        inline std::vector<Token> Tokenize(
            const std::string& line)
        {
            // Tokenization is whitespace-based. `#` starts a comment anywhere on
            // the line, which makes scene files double as readable render notes.
            std::vector<Token> tokens;
            std::size_t i = 0;

            while (i < line.size())
            {
                if (line[i] == '#')
                {
                    break;
                }

                if (line[i] == ' ' || line[i] == '\t' || line[i] == '\r')
                {
                    ++i;
                    continue;
                }

                const std::size_t begin = i;
                while (i < line.size() &&
                       line[i] != ' ' &&
                       line[i] != '\t' &&
                       line[i] != '\r' &&
                       line[i] != '#')
                {
                    ++i;
                }

                tokens.push_back(
                    Token
                    {
                        line.substr(begin, i - begin),
                        static_cast<std::uint32_t>(begin + 1)
                    });
            }

            return tokens;
        }

        [[noreturn]]
        inline void Fail(
            std::uint32_t line,
            std::uint32_t column,
            std::string message)
        {
            throw SceneParseException(
                SceneParseError
                {
                    line,
                    column,
                    std::move(message)
                });
        }

        inline void RequireCount(
            const std::vector<Token>& tokens,
            std::uint32_t line,
            std::size_t expected,
            std::string_view usage)
        {
            // Command arity is strict on purpose. A renderer should fail loudly
            // when a scene says something ambiguous instead of guessing.
            if (tokens.size() != expected)
            {
                const std::uint32_t column =
                    tokens.empty() ? 1u : tokens.back().Column;

                Fail(
                    line,
                    column,
                    "expected " + std::to_string(expected) +
                    " tokens for `" + std::string(usage) +
                    "`, got " + std::to_string(tokens.size()) + ".");
            }
        }

        [[nodiscard]]
        inline float ParseFloat(
            const Token& token,
            std::uint32_t line)
        {
            // std::from_chars avoids locale surprises and lets us verify that
            // the whole token was consumed. "1abc" should not parse as 1.
            float value = 0.0f;
            const char* begin = token.Text.data();
            const char* end = begin + token.Text.size();
            const auto [ptr, ec] = std::from_chars(begin, end, value);
            if (ec != std::errc{} || ptr != end)
            {
                Fail(line, token.Column, "invalid float `" + token.Text + "`.");
            }

            return value;
        }

        [[nodiscard]]
        inline std::uint32_t ParseUInt(
            const Token& token,
            std::uint32_t line)
        {
            std::uint32_t value = 0;
            const char* begin = token.Text.data();
            const char* end = begin + token.Text.size();
            const auto [ptr, ec] = std::from_chars(begin, end, value);
            if (ec != std::errc{} || ptr != end)
            {
                Fail(line, token.Column, "invalid unsigned integer `" + token.Text + "`.");
            }

            return value;
        }

        [[nodiscard]]
        inline Color3f ParseColor(
            const std::vector<Token>& tokens,
            std::uint32_t line,
            std::size_t offset)
        {
            return
            {
                ParseFloat(tokens.at(offset), line),
                ParseFloat(tokens.at(offset + 1), line),
                ParseFloat(tokens.at(offset + 2), line)
            };
        }

        [[nodiscard]]
        inline Vec3f ParseVec3(
            const std::vector<Token>& tokens,
            std::uint32_t line,
            std::size_t offset)
        {
            return
            {
                ParseFloat(tokens.at(offset), line),
                ParseFloat(tokens.at(offset + 1), line),
                ParseFloat(tokens.at(offset + 2), line)
            };
        }

        [[nodiscard]]
        inline RenderMode ParseRenderMode(
            const Token& token,
            std::uint32_t line)
        {
            if (token.Text == "normal") return RenderMode::Normal;
            if (token.Text == "depth") return RenderMode::Depth;
            if (token.Text == "whitted") return RenderMode::Whitted;
            if (token.Text == "shadow_mask") return RenderMode::ShadowMask;
            if (token.Text == "bvh_heatmap") return RenderMode::BVHHeatmap;
            if (token.Text == "albedo") return RenderMode::Albedo;
            if (token.Text == "primitive_id") return RenderMode::PrimitiveID;
            if (token.Text == "uv") return RenderMode::UV;
            if (token.Text == "barycentric") return RenderMode::Barycentric;
            if (token.Text == "accel_diff") return RenderMode::AccelerationDifference;
            if (token.Text == "pbr") return RenderMode::PBR;

            Fail(line, token.Column, "unknown integrator `" + token.Text + "`.");
        }

        [[nodiscard]]
        inline MaterialType ParseMaterialType(
            const Token& token,
            std::uint32_t line)
        {
            if (token.Text == "lambert") return MaterialType::Lambert;
            if (token.Text == "mirror") return MaterialType::Mirror;
            if (token.Text == "glass") return MaterialType::Glass;
            if (token.Text == "pbr") return MaterialType::PBR;
            if (token.Text == "emissive") return MaterialType::Emissive;

            Fail(line, token.Column, "unknown material type `" + token.Text + "`.");
        }
    }

    [[nodiscard]]
    inline Scene ParseSceneText(
        const std::string& text,
        const std::filesystem::path& baseDirectory = std::filesystem::current_path())
    {
        // First pass and construction happen together because V1 references only
        // earlier material names. This keeps the grammar simple: define material
        // before using it.
        Scene scene;
        std::unordered_map<std::string, std::uint32_t> materialIndices;

        bool cameraFound = false;
        Vec3f cameraPosition = Vec3f{ 0.0f, 1.0f, 6.0f };
        Vec3f cameraTarget = Vec3f{ 0.0f, 1.0f, 0.0f };
        Vec3f cameraUp = Vec3f::Up();
        float cameraFOV = 45.0f;

        std::istringstream stream(text);
        std::string lineText;
        std::uint32_t line = 0;

        while (std::getline(stream, lineText))
        {
            ++line;
            const std::vector<parser_detail::Token> tokens =
                parser_detail::Tokenize(lineText);

            if (tokens.empty())
            {
                continue;
            }

            const std::string& command =
                tokens[0].Text;

            if (command == "resolution")
            {
                parser_detail::RequireCount(tokens, line, 3, "resolution width height");
                scene.Settings.Width = parser_detail::ParseUInt(tokens[1], line);
                scene.Settings.Height = parser_detail::ParseUInt(tokens[2], line);
                if (scene.Settings.Width == 0 || scene.Settings.Height == 0)
                {
                    parser_detail::Fail(line, tokens[1].Column, "resolution dimensions must be non-zero.");
                }
            }
            else if (command == "samples")
            {
                parser_detail::RequireCount(tokens, line, 2, "samples count");
                scene.Settings.SamplesPerPixel = parser_detail::ParseUInt(tokens[1], line);
                if (scene.Settings.SamplesPerPixel == 0)
                {
                    parser_detail::Fail(line, tokens[1].Column, "sample count must be non-zero.");
                }
            }
            else if (command == "background")
            {
                parser_detail::RequireCount(tokens, line, 4, "background r g b");
                scene.Settings.Background = parser_detail::ParseColor(tokens, line, 1);
            }
            else if (command == "integrator")
            {
                parser_detail::RequireCount(tokens, line, 2, "integrator mode");
                scene.Settings.Mode = parser_detail::ParseRenderMode(tokens[1], line);
            }
            else if (command == "camera")
            {
                parser_detail::RequireCount(tokens, line, 11, "camera px py pz tx ty tz upx upy upz fov");
                cameraPosition = parser_detail::ParseVec3(tokens, line, 1);
                cameraTarget = parser_detail::ParseVec3(tokens, line, 4);
                cameraUp = parser_detail::ParseVec3(tokens, line, 7);
                cameraFOV = parser_detail::ParseFloat(tokens[10], line);
                cameraFound = true;
            }
            else if (command == "material")
            {
                // Emissive materials may provide explicit emission. If omitted,
                // albedo becomes the visible emission color so small demo scenes
                // can be concise.
                if (tokens.size() != 6 && tokens.size() != 7 && tokens.size() != 8 && tokens.size() != 9)
                {
                    parser_detail::Fail(line, tokens[0].Column, "usage: material name type r g b [ior | roughness metallic | emitR emitG emitB].");
                }

                Material material;
                material.Name = tokens[1].Text;
                material.Type = parser_detail::ParseMaterialType(tokens[2], line);
                material.Albedo = parser_detail::ParseColor(tokens, line, 3);
                if (material.Type == MaterialType::Glass && tokens.size() == 7)
                {
                    material.IOR = parser_detail::ParseFloat(tokens[6], line);
                }

                if (material.Type == MaterialType::PBR && tokens.size() == 8)
                {
                    material.Roughness = parser_detail::ParseFloat(tokens[6], line);
                    material.Metallic = parser_detail::ParseFloat(tokens[7], line);
                }

                material.Emission = tokens.size() == 9
                    ? parser_detail::ParseColor(tokens, line, 6)
                    : Color3f::Black();

                if (material.Type == MaterialType::Emissive && !IsNonBlack(material.Emission))
                {
                    material.Emission = material.Albedo;
                }

                if (materialIndices.contains(material.Name))
                {
                    parser_detail::Fail(line, tokens[1].Column, "duplicate material `" + material.Name + "`.");
                }

                const std::string materialName =
                    material.Name;

                materialIndices[materialName] =
                    scene.AddMaterial(std::move(material));
            }
            else if (command == "light")
            {
                parser_detail::RequireCount(tokens, line, 9, "light point x y z r g b intensity");
                if (tokens[1].Text != "point")
                {
                    parser_detail::Fail(line, tokens[1].Column, "only point lights are supported in V1.");
                }

                scene.Lights.push_back(
                    PointLight
                    {
                        parser_detail::ParseVec3(tokens, line, 2),
                        parser_detail::ParseColor(tokens, line, 5),
                        parser_detail::ParseFloat(tokens[8], line)
                    });
            }
            else if (command == "sphere")
            {
                parser_detail::RequireCount(tokens, line, 6, "sphere x y z radius material");
                const auto materialIt =
                    materialIndices.find(tokens[5].Text);

                if (materialIt == materialIndices.end())
                {
                    parser_detail::Fail(line, tokens[5].Column, "unknown material `" + tokens[5].Text + "`.");
                }

                scene.AddSphere(
                    parser_detail::ParseVec3(tokens, line, 1),
                    parser_detail::ParseFloat(tokens[4], line),
                    materialIt->second);
            }
            else if (command == "triangle")
            {
                parser_detail::RequireCount(tokens, line, 11, "triangle ax ay az bx by bz cx cy cz material");
                const auto materialIt =
                    materialIndices.find(tokens[10].Text);

                if (materialIt == materialIndices.end())
                {
                    parser_detail::Fail(line, tokens[10].Column, "unknown material `" + tokens[10].Text + "`.");
                }

                scene.AddTriangle(
                    parser_detail::ParseVec3(tokens, line, 1),
                    parser_detail::ParseVec3(tokens, line, 4),
                    parser_detail::ParseVec3(tokens, line, 7),
                    materialIt->second);
            }
            else if (command == "obj")
            {
                parser_detail::RequireCount(tokens, line, 7, "obj path material scale tx ty tz");
                const auto materialIt =
                    materialIndices.find(tokens[2].Text);

                if (materialIt == materialIndices.end())
                {
                    parser_detail::Fail(line, tokens[2].Column, "unknown material `" + tokens[2].Text + "`.");
                }

                std::filesystem::path meshPath =
                    tokens[1].Text;

                if (meshPath.is_relative())
                {
                    meshPath = baseDirectory / meshPath;
                }

                const TriangleMesh mesh =
                    LoadOBJMesh(
                        meshPath,
                        parser_detail::ParseFloat(tokens[3], line),
                        parser_detail::ParseVec3(tokens, line, 4));

                for (const Trianglef& triangle : mesh.Triangles)
                {
                    scene.AddTriangle(
                        triangle.A,
                        triangle.B,
                        triangle.C,
                        materialIt->second);
                }
            }
            else
            {
                parser_detail::Fail(line, tokens[0].Column, "unknown command `" + command + "`.");
            }
        }

        if (!cameraFound)
        {
            parser_detail::Fail(1, 1, "scene is missing a camera command.");
        }

        if (scene.Materials.empty())
        {
            parser_detail::Fail(1, 1, "scene must define at least one material.");
        }

        if (scene.Primitives.empty())
        {
            parser_detail::Fail(1, 1, "scene must define at least one primitive.");
        }

        scene.MainCamera =
            Camera::LookAt(
                cameraPosition,
                cameraTarget,
                cameraUp,
                cameraFOV,
                static_cast<float>(scene.Settings.Width) /
                    static_cast<float>(scene.Settings.Height));

        // Build the BVH after all primitives are known. If parsing fails before
        // this point, no half-built scene escapes to the renderer.
        scene.BuildAcceleration();
        return scene;
    }

    [[nodiscard]]
    inline Scene LoadScene(
        const std::filesystem::path& path)
    {
        std::ifstream in(path);
        if (!in)
        {
            throw std::runtime_error("Failed to open scene file: " + path.string());
        }

        std::ostringstream buffer;
        buffer << in.rdbuf();
        return ParseSceneText(
            buffer.str(),
            path.has_parent_path()
                ? path.parent_path()
                : std::filesystem::current_path());
    }
}
