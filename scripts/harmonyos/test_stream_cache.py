#!/usr/bin/env python3
"""Host-only cache identity/eviction tests. Use the README's per-command Xcode 27 override."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
core = root / 'apps/harmonyos/entry/src/main/cpp'
for name in ('upload_rows', 'scene_cache', 'page_atlas', 'paged_scene', 'lod_selection', 'decode_queue'):
    output = root / 'artifacts/harmonyos' / (name + '-test')
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I', str(core), '-I', str(core / 'third_party'),
                    str(root / 'apps/harmonyos/tests' / (name + '_test.cpp')), *([str(core / 'splat.cpp')] if name == 'paged_scene' else []), '-o', str(output)], check=True)
    subprocess.run([str(output)], check=True, timeout=30)
