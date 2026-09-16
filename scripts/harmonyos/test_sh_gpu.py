"""Run production SH GLSL on the Mac GPU against independent harmonic recurrence.

Use the README's per-command Xcode override. This is not phone acceptance.
"""
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='nextnews-sh-gpu-') as temporary:
    executable = Path(temporary) / 'sh-gpu-test'
    subprocess.run([
        'xcrun', 'clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
        '-I', str(root / 'apps/harmonyos/entry/src/main/cpp'),
        str(root / 'apps/harmonyos/tests/sh_gpu_test.cpp'),
        '-framework', 'OpenGL', '-o', str(executable)
    ], check=True)
    subprocess.run([str(executable)], check=True)
