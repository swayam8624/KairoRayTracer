module;

#include <stdexcept>

export module Kairo.Foundation.RayTracer.Shading;

import Kairo.Foundation.RayTracer.Color;
import Kairo.Foundation.RayTracer.Material;
import Kairo.Foundation.RayTracer.Scene;
import Kairo.Foundation.RayTracer.Texture;
import Kairo.Foundation.RayTracer.Types;

export namespace kairo::foundation::raytracer
{
    //=========================================================
    // Shading Helpers
    //
    // Integrators should decide how light travels. They should not each invent
    // their own material lookup rules. These helpers centralize renderer-wide
    // conventions such as "texture * albedo tint" and scene miss radiance.
    //=========================================================

    [[nodiscard]]
    inline const Material& MaterialForHit(
        const Scene& scene,
        const SurfaceHit& hit)
    {
        if (hit.MaterialIndex >= scene.Materials.size())
        {
            throw std::out_of_range("Surface hit references a material index that does not exist.");
        }

        return scene.Materials.at(hit.MaterialIndex);
    }

    [[nodiscard]]
    inline Color3f EvaluateAlbedo(
        const Scene& scene,
        const SurfaceHit& hit)
    {
        const Material& material =
            MaterialForHit(scene, hit);

        if (!material.HasAlbedoTexture())
        {
            return material.Albedo;
        }

        if (material.AlbedoTextureIndex >= scene.Textures.size())
        {
            throw std::out_of_range("Material references a texture index that does not exist.");
        }

        return material.Albedo *
            SampleTexture(
                scene.Textures.at(material.AlbedoTextureIndex),
                hit.UV);
    }
}
