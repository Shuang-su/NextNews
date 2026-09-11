#!/usr/bin/env python3
"""Run after a fresh app launch, with scripts/harmonyos/env.sh sourced."""
import datetime
import json
import os
from pathlib import Path
import subprocess
import time

out = Path('artifacts/harmonyos/toolbar-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
out.mkdir(parents=True)
hdc = str(Path(os.environ['DEVECO_STUDIO_HOME']) / 'Contents/sdk/default/openharmony/toolchains/hdc') if 'DEVECO_STUDIO_HOME' in os.environ else '/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc'

def run(args):
    return subprocess.run(args, check=True, capture_output=True, text=True, timeout=60).stdout

def ui(*args):
    return run(['devecocli', 'ui', *args])

def nodes(tree):
    for node in tree:
        yield node
        yield from nodes(node.get('children', []))

def layout():
    return json.loads(ui('layout', '--format', 'json'))

def icon(label):
    ui('click', '--id', label)
    time.sleep(.4)

def click(label):
    node = next(node for node in nodes(layout()) if node.get('text') == label)
    x, y, right, bottom = node['bounds']
    ui('click', str((x + right) // 2), str((y + bottom) // 2))
    time.sleep(.4)

def key(code):
    run([hdc, 'shell', 'uinput', '-K', '-d', str(code), '-i', '100', '-u', str(code)])
    time.sleep(.5)

def snapshot(name):
    (out / (name + '.json')).write_text(json.dumps(layout(), ensure_ascii=False, indent=2))
    ui('screenshot', '--path', str(out / (name + '.png')))

icon('模型')
click('公开样例')
icon('模型')
icon('复位')
key(2022)  # F
assert any(node.get('id') == '显示工具' for node in nodes(layout()))
key(2022)
key(2024)  # H
assert any(node.get('text') == '浏览操作' for node in nodes(layout()))
key(2024)
# Tap in the unoccluded center area, independent of device density.
surface = next(node for node in nodes(layout()) if node.get('id') == 'viewer-surface')
x, y, right, bottom = surface['bounds']
ui('doubleclick', str((x + right) // 2), str(y + (bottom - y) // 3))
time.sleep(.7)
assert any(node.get('text') == '飞行 · 拖动转头' for node in nodes(layout()))
snapshot('flight')
icon('环绕')
icon('复位')
snapshot('orbit')
print('PASS F immersive, H help, double-click flight, reset; evidence:', out)
