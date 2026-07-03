module;

#include <cstdint>
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
    // - Emissive: visible light-emitting surface.
    // Roughness/metallic/IOR are stored now so the file format can grow toward
    // PBR/path tracing without replacing the public material record.
    //=========================================================

    enum class MaterialType : std::uint8_t
    {
        Lambert,
        Mirror,
        Glass,
        Emissive
    };

    struct Material final
    {
        std::string Name = "material";
        MaterialType Type = MaterialType::Lambert;
        Color3f Albedo = Color3f::White();
        Color3f Emission = Color3f::Black();
        float Roughness = 0.0f;
        float Metallic = 0.0f;
        float IOR = 1.5f;
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
        case MaterialType::Emissive: return "emissive";
        }

        return "unknown";
    }
}
