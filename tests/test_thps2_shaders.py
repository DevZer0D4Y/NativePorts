"""Compile and link the real Dreamcast pipeline shaders across GL/ES variants."""
from pathlib import Path
import os, re, subprocess

root = Path(__file__).resolve().parents[1]
work = root.parent / 'thps2-shader-validation'
work.mkdir(exist_ok=True)
source = (root / 'vendor/flycast/core/rend/gles/gles.cpp').read_text()
templates = {}
for name in ['VertexShaderSource', 'PixelPipelineShader', 'ModifierVolumeShader']:
    templates[name] = re.search(r'const char\*\s*' + name + r'\s*=\s*R"\((.*?)\)";', source, re.S).group(1)
validator = root.parent / 'tools/glslang/glslangValidator'
env = dict(os.environ, DYLD_LIBRARY_PATH=str(validator.parent / 'lib'))
cases = [[0] * 14, [1] * 14]
for index in range(14):
    values = [0] * 14
    values[index] = 1
    cases.append(values)
for index in [1, 5, 7]:
    for value in [2, 3]:
        values = [1] * 14
        values[index] = value
        cases.append(values)
count = 0
for mode, version in [('GLES3', '#version 300 es'), ('GLES2', '#version 100'), ('GL3', '#version 130'), ('GL2', '#version 120')]:
    for index, values in enumerate(cases):
        vert = work / (mode + '-' + str(index) + '.vert')
        frag = work / (mode + '-' + str(index) + '.frag')
        vert.write_text(templates['VertexShaderSource'] % (version, mode, values[8]))
        frag.write_text(templates['PixelPipelineShader'] % tuple([version, mode] + values))
        result = subprocess.run([str(validator), '-l', str(vert), str(frag)], env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f'{mode} pipeline {index}:\n{result.stdout}')
        count += 1
    vert.write_text(templates['VertexShaderSource'] % (version, mode, 1))
    frag.write_text(templates['ModifierVolumeShader'] % (version, mode))
    result = subprocess.run([str(validator), '-l', str(vert), str(frag)], env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'{mode} modifier volume:\n{result.stdout}')
    count += 1
print(f'{count} Dreamcast shader program variants compiled and linked successfully.')
