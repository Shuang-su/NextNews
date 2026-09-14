#!/usr/bin/env python3
"""Real-device comparison. Run from repo root after sourcing env.sh; no host FPS claims."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--device', help='HDC target; otherwise exactly one connected target is required')
parser.add_argument('--backend', choices=['both', 'gl', 'spatial'], default='both')
parser.add_argument('--out', type=Path, default=Path('artifacts/harmonyos') / ('comparison-' + time.strftime('%Y%m%d-%H%M%S')))
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=False)
hdc = str(Path(os.environ.get('DEVECO_STUDIO_HOME', '/Applications/DevEco-Studio.app')) / 'Contents/sdk/default/openharmony/toolchains/hdc')

def run(command):
    return subprocess.run(command, check=True, capture_output=True, text=True, timeout=60).stdout

if not args.device:
    targets = [v for v in run([hdc, 'list', 'targets']).splitlines() if v.strip() and v.strip() != '[Empty]']
    if len(targets) != 1:
        raise SystemExit('Connect exactly one device or specify --device')
    args.device = targets[0].strip()

def shell(command):
    return run([hdc, '-t', args.device, 'shell', command])

def ui(*options):
    return run(['devecocli', 'ui', *options, '--device', args.device])

def nodes(tree):
    for node in tree:
        yield node
        yield from nodes(node.get('children', []))

def layout():
    return json.loads(ui('layout', '--format', 'json'))

def click(label, icon=False):
    if icon:
        ui('click', '--id', label)
    else:
        node = next(n for n in nodes(layout()) if n.get('text') == label)
        x, y, right, bottom = node['bounds']
        ui('click', str((x + right) // 2), str((y + bottom) // 2))
    time.sleep(.5)

def snapshot(name):
    tree = layout()
    (args.out / (name + '.json')).write_text(json.dumps(tree, ensure_ascii=False, indent=2))
    ui('screenshot', '--path', str(args.out / (name + '.png')))

def rs(option):
    return shell('hidumper -s 10 -a "' + option + '"')

def measure(name, backend):
    tree = rs('RSTree')
    (args.out / (name + '-tree.txt')).write_text(tree)
    surfaces = {int(i): n for i, n in re.findall(r'SURFACE_NODE\[(\d+)\][^\n]*?Name \[([^]]+)\]', tree)}
    expected = 'SceneViewer Model0' if backend == 'spatial' else 'nextnews-splatSurface'
    target = next(i for i, n in surfaces.items() if n == expected)
    # Save both timestamp columns without assuming that either represents GPU duration.
    (args.out / (name + '-before-rs.txt')).write_text(rs(f'-id {target} fps'))
    with (args.out / (name + '-perf.txt')).open('w') as log:
        collector = subprocess.Popen([hdc, '-t', args.device, 'shell',
            'SP_daemon -N 12 -PKG com.nextnews.splatviewer -f -g -r -OUT /data/local/tmp/nextnews_comparison.csv'],
            stdout=log, stderr=subprocess.STDOUT)
        try:
            click('测量 10 秒')
            time.sleep(5)
            (args.out / (name + '-rs.txt')).write_text(rs(f'-id {target} fps'))
            collector.wait(timeout=30)
        finally:
            if collector.poll() is None:
                collector.terminate()
                collector.wait(timeout=5)
    after = (args.out / (name + '-rs.txt')).read_text()
    before = (args.out / (name + '-before-rs.txt')).read_text()
    timestamps = sorted(set(int(b) for _, b in re.findall(r'^(\d+):(\d+)$', after, re.M)))
    old = [int(b) for _, b in re.findall(r'^(\d+):(\d+)$', before, re.M)]
    recent = [t for t in timestamps if t >= timestamps[-1] - 1_000_000_000] if timestamps else []
    fresh = bool(timestamps and (not old or timestamps[-1] > max(old)))
    frequency = ((len(recent) - 1) * 1e9 / (recent[-1] - recent[0])) if fresh and len(recent) > 1 else None
    raw = (args.out / (name + '-perf.txt')).read_text()
    result = {'backend': backend, 'sample': name, 'surface': expected,
              'fresh_surface_records': fresh, 'rs_last_second_record_hz': frequency,
              'smartperf_fps_samples': [int(v) for v in re.findall(r' fps=(\d+)', raw)],
              'process_pss_kib_samples': [int(v) for v in re.findall(r' pss=(\d+)', raw)],
              'note': 'RS record frequency is not GPU duration; SmartPerf GPU load is device-wide. See comparison report.'}
    (args.out / (name + '-metrics.json')).write_text(json.dumps(result, indent=2))
    if not fresh or frequency is None or len(result['smartperf_fps_samples']) != 12:
        raise RuntimeError('Incomplete or stale device metrics for ' + name)
    snapshot(name + '-benchmark')
    print(name + ' captured', flush=True)

for backend in (['spatial', 'gl'] if args.backend == 'both' else [args.backend]):
    shell('aa force-stop com.nextnews.splatviewer')
    shell('aa start -b com.nextnews.splatviewer -a EntryAbility')
    time.sleep(3)
    click('模型', icon=True)
    if backend == 'spatial':
        click('华为原生渲染')
        time.sleep(2)
    labels = [('公开样例', 'public'), ('Tripo', 'tripo'),
              ('转换产物' if backend == 'spatial' else '转换样例', 'converted')]
    for index, (label, model) in enumerate(labels):
        if backend == 'gl' and index:
            click('模型', icon=True)
        click(label)
        time.sleep(2)
        if backend == 'gl':
            click('模型', icon=True)
            click('复位', icon=True)
        snapshot(backend + '-' + model)
        if backend == 'gl':
            click('设置', icon=True)
            click('性能信息')
        measure(backend + '-' + model, backend)
