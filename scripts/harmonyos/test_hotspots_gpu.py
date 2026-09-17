"""Compile the production hotspot class on desktop GL with only platform shims.

Not a substitute for phone shader/rendering verification.
"""
import subprocess
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[2]
core = root / 'apps/harmonyos/entry/src/main/cpp'
with tempfile.TemporaryDirectory(prefix='nextnews-hotspot-gpu-') as temporary:
    target = Path(temporary)
    (target / 'GLES3').mkdir()
    (target / 'GLES3/gl3.h').write_text('#define GL_SILENCE_DEPRECATION\n#include <OpenGL/gl3.h>\n')
    (target / 'hilog').mkdir()
    (target / 'hilog/log.h').write_text('#define OH_LOG_Print(...) ((void)0)\n')
    # ESSL and desktop GLSL share the tested math. Do not rewrite shader logic.
    (target / 'hotspots.cpp').write_text((core / 'hotspots.cpp').read_text().replace('#version 300 es', '#version 410 core'))
    executable = target / 'test'
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-I', str(target), '-I', str(core), str(target / 'hotspots.cpp'),
                    str(root / 'apps/harmonyos/tests/hotspots_gpu_test.cpp'),
                    '-framework', 'OpenGL', '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
