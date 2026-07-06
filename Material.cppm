module;

#include <cstdint>
#include <limits>
#include <string>

export module Kairo.Foundation.RayTracer.Material;

import Kairo.Foundation.RayTracer.Color;

export namespace kairo::foundation::raytracer
{
    //=========================================================
    // Materials
    //
    // V1 materials are intentionally small:
    // - Lambert: matte diffuse surface.
    // - Mirror: perfect specular reflection.
    // - Glass: reflection plus refraction using Schlick Fresnel.
    // - PBR: Cook-Torrance/GGX direct lighting with roughness/metallic.
    // - Emissive: visible light-emitting surface.
    // Roughness/metallic/IOR are stored now so the file format can grow toward
    // PBR/path tracing without replacing the public material record.
    //=========================================================

    enum class MaterialType : std::uint8_t
    {
        Lambert,
        Mirror,
        Glass,
        PBR,
        Emissive
    };

    struct Material final
    {
        std::string Name = "material";
        MaterialType Type = MaterialType::Lambert;
        Color3f Albedo = Color3f::White();
        Color3f Emission = Color3f::Black();
        std::uint32_t AlbedoTextureIndex = std::numeric_limits<std::uint32_t>::max();
        float Roughness = 0.0f;
        float Metallic = 0.0f;
        float IOR = 1.5f;

        [[nodiscard]]
        bool HasAlbedoTexture() const noexcept
        {
            return AlbedoTextureIndex != std::numeric_limits<std::uint32_t>::max();
        }
    };

    [[nodiscard]]
    inline const char* ToString(
        MaterialType type) noexcept
    {
        switch (type)
        {
        case MaterialType::Lambert: return "lambert";
        case MaterialType::Mirror: return "mirror";
        case MaterialType::Glass: return "glass";
        case MaterialType::PBR: return "pbr";
        case MaterialType::Emissive: return "emissive";
        }

        return "unknown";
    }
}
