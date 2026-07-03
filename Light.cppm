module;

export module Kairo.Foundation.RayTracer.Light;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.RayTracer.Color;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;

    struct PointLight final
    {
        Vec3f Position = Vec3f::Zero();
        Color3f Color = Color3f::White();
        float Intensity = 1.0f;
    };
}
