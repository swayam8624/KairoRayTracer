from pathlib import Path
p=Path('PathIntegrator.cppm')
s=p.read_text()
if 'Color3f directLighting' in s and 'for (const DirectionalLight& light : scene.DirectionalLights)' in s:
    start=s.index('        for (const DirectionalLight& light : scene.DirectionalLights)')
    end=s.index('        for (const AreaLight& light : scene.AreaLights)', start)
    block=s[start:end].replace('            color += ', '            directLighting += ')
    s=s[:start]+block+s[end:]
p.write_text(s)
Path('.github/workflows/normalize-path-light-accumulator.yml').unlink()
Path('.github/scripts/normalize_path_light_accumulator.py').unlink()
