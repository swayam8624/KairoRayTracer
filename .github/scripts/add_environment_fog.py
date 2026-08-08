from pathlib import Path

p = Path('RayTracerTypes.cppm')
s = p.read_text()
if '#include <cmath>' not in s:
    s = s.replace('#include <cstdint>', '#include <cmath>\n#include <cstdint>')
s = s.replace('    enum class AccelerationMode : std::uint8_t\n', '''    enum class FogMode : std::uint8_t\n    {\n        Disabled,\n        Linear,\n        Exponential\n    };\n\n    enum class AccelerationMode : std::uint8_t\n''')
s = s.replace('        Color3f Background = { 0.02f, 0.03f, 0.05f };\n', '''        Color3f Background = { 0.02f, 0.03f, 0.05f };\n        FogMode Fog = FogMode::Disabled;\n        Color3f FogColor = { 0.5f, 0.5f, 0.5f };\n        float FogDensity = 0.01f;\n        float FogNear = 0.0f;\n        float FogFar = 1000.0f;\n''')
needle = '''        if (settings.MaxDepth > 64u)\n        {\n            throw std::invalid_argument("MaxDepth is too large; maximum supported value is 64.");\n        }\n'''
insert = needle + '''\n        if (!std::isfinite(settings.FogDensity) || settings.FogDensity < 0.0f ||\n            !std::isfinite(settings.FogNear) || !std::isfinite(settings.FogFar) ||\n            settings.FogNear < 0.0f || settings.FogFar <= settings.FogNear)\n        {\n            throw std::invalid_argument("Fog settings must have finite non-negative density and a valid near/far range.");\n        }\n'''
if needle not in s: raise SystemExit('render settings validation anchor missing')
s = s.replace(needle, insert)
p.write_text(s)

p = Path('Renderer.cppm')
s = p.read_text()
helper_anchor = '''    [[nodiscard]]\n    inline Color3f TraceMode(\n'''
helper = '''    [[nodiscard]]\n    inline Color3f ApplySceneFog(\n        const Scene& scene,\n        const Rayf& ray,\n        Color3f shaded)\n    {\n        if (scene.Settings.Fog == FogMode::Disabled) return shaded;\n        const auto hit = scene.Intersect(ray, nullptr);\n        const float distance = hit ? hit->Distance : scene.Settings.FogFar;\n        float amount = 0.0f;\n        if (scene.Settings.Fog == FogMode::Linear)\n        {\n            const float span = scene.Settings.FogFar - scene.Settings.FogNear;\n            amount = std::clamp((distance - scene.Settings.FogNear) / span, 0.0f, 1.0f);\n        }\n        else\n        {\n            amount = std::clamp(1.0f - std::exp(-scene.Settings.FogDensity * distance), 0.0f, 1.0f);\n        }\n        return shaded * (1.0f - amount) + scene.Settings.FogColor * amount;\n    }\n\n'''
if helper_anchor not in s: raise SystemExit('renderer helper anchor missing')
s = s.replace(helper_anchor, helper + helper_anchor, 1)
old = '''                color +=\n                    TraceMode(\n                        scene,\n                        ray,\n                        &localStats,\n                        rng);'''
new = '''                Color3f sampleColor =\n                    TraceMode(\n                        scene,\n                        ray,\n                        &localStats,\n                        rng);\n                if (scene.Settings.Mode == RenderMode::Whitted ||\n                    scene.Settings.Mode == RenderMode::PBR ||\n                    scene.Settings.Mode == RenderMode::Path)\n                    sampleColor = ApplySceneFog(scene, ray, sampleColor);\n                color += sampleColor;'''
if old not in s: raise SystemExit('RenderPixel sample anchor missing')
s = s.replace(old, new, 1)
p.write_text(s)

Path('.github/workflows/add-environment-fog.yml').unlink()
Path('.github/scripts/add_environment_fog.py').unlink()
