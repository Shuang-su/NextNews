#!/usr/bin/env python3
"""Exact stable depth-order regression; optional optimized host timing (not a phone gate)."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--count', type=int, default=100003)
parser.add_argument('--timing', action='store_true')
args = parser.parse_args()
if not 1 <= args.count <= 8000000:
    parser.error('count must be in [1, 8000000]')
root = Path(__file__).resolve().parents[2]
core = root / 'apps/harmonyos/entry/src/main/cpp'
out = root / 'artifacts/harmonyos/sort-test'
out.parent.mkdir(parents=True, exist_ok=True)
flags = ['-O2'] if args.timing else ['-g', '-fsanitize=address,undefined']
subprocess.run([os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                *flags, '-I', str(core), str(core / 'splat.cpp'),
                str(root / 'apps/harmonyos/tests/sort_test.cpp'), '-o', str(out)], check=True)
subprocess.run([str(out), str(args.count)], check=True)
