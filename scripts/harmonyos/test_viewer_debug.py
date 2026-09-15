#!/usr/bin/env python3
"""Bounded native collision wire extraction. Use the README Xcode override."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];core=root/'apps/harmonyos/entry/src/main/cpp';out=root/'artifacts/harmonyos/debug-test'
out.parent.mkdir(parents=True,exist_ok=True)
subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I',str(core),str(core/'collision/collision.cpp'),str(core/'collision/mesh.cpp'),str(root/'apps/harmonyos/tests/debug_test.cpp'),'-o',str(out)],check=True)
subprocess.run([str(out)],check=True)
