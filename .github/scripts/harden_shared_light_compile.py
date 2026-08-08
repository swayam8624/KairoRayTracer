from pathlib import Path

for name in ['Light.cppm','PBRIntegrator.cppm','WhittedIntegrator.cppm','PathIntegrator.cppm']:
    p=Path(name)
    s=p.read_text()
    s=s.replace('Vec3f::Forward()', 'Vec3f{ 0.0f, 0.0f, -1.0f }')
    s=s.replace('-Vec3f::Up()', 'Vec3f{ 0.0f, -1.0f, 0.0f }')
    p.write_text(s)

p=Path('Renderer.cppm')
s=p.read_text()
if '#include <algorithm>' not in s:
    s=s.replace('module;\n', 'module;\n\n#include <algorithm>\n')
if '#include <cmath>' not in s:
    s=s.replace('#include <algorithm>\n', '#include <algorithm>\n#include <cmath>\n')
p.write_text(s)

Path('.github/workflows/harden-shared-light-compile.yml').unlink()
Path('.github/scripts/harden_shared_light_compile.py').unlink()
