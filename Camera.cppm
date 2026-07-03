module;

#include <cmath>
#include <numbers>
#include <stdexcept>

export module Kairo.Foundation.RayTracer.Camera;

import Kairo.Foundation.Math.Vector;
import Kairo.Foundation.Geometry.Ray;

export namespace kairo::foundation::raytracer
{
    using namespace kairo::foundation::math;
    using namespace kairo::foundation::geometry;

    class Camera final
    {
    public:
        Vec3f Position = Vec3f::Zero();
        Vec3f Forward = Vec3f::Forward();
        Vec3f Right = Vec3f::UnitX();
        Vec3f Up = Vec3f::Up();
        float VerticalFOVDegrees = 45.0f;
        float AspectRatio = 1.0f;

        /// Input: position, target, world up, field-of-view in degrees, and aspect ratio.
        /// Output: camera basis suitable for primary-ray generation.
        /// Task: convert scene-file camera data into normalized view vectors.
        [[nodiscard]]
        static Camera LookAt(
            const Vec3f& position,
            const Vec3f& target,
            const Vec3f& worldUp,
            float verticalFOVDegrees,
            float aspectRatio)
        {
            if (verticalFOVDegrees <= 0.0f || verticalFOVDegrees >= 179.0f)
            {
                throw std::invalid_argument("Camera FOV must be in the open range (0, 179).");
            }

            if (aspectRatio <= 0.0f)
            {
                throw std::invalid_argument("Camera aspect ratio must be positive.");
            }

            Camera camera;
            camera.Position = position;
            camera.Forward = SafeNormalize(target - position, Vec3f::Forward());
            camera.Right = SafeNormalize(Cross(camera.Forward, worldUp), Vec3f::UnitX());
            camera.Up = SafeNormalize(Cross(camera.Right, camera.Forward), Vec3f::Up());
            camera.VerticalFOVDegrees = verticalFOVDegrees;
            camera.AspectRatio = aspectRatio;
            return camera;
        }

        /// Input: normalized raster coordinates in [0, 1].
        /// Output: normalized camera ray through the film plane.
        /// Task: map pixels into world-space rays for all integrators.
        [[nodiscard]]
        Rayf GenerateRay(
            float u,
            float v) const noexcept
        {
            const float radians =
                VerticalFOVDegrees * std::numbers::pi_v<float> / 180.0f;

            const float filmHalfHeight =
                std::tan(radians * 0.5f);

            const float filmHalfWidth =
                filmHalfHeight * AspectRatio;

            const float screenX =
                (2.0f * u - 1.0f) * filmHalfWidth;

            const float screenY =
                (1.0f - 2.0f * v) * filmHalfHeight;

            const Vec3f direction =
                SafeNormalize(
                    Forward + Right * screenX + Up * screenY,
                    Forward);

            return Rayf::FromOriginDirection(Position, direction);
        }
    };
}
