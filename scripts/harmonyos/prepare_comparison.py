#!/usr/bin/env python3
"""Prepare same-count PLY and upstream browser comparison files, preserving sources.
Run after prepare_web_reference.sh. No source splat or viewer-settings is changed.
"""
import argparse,json,shutil,subprocess
from pathlib import Path
from prepare_model import prepare,sha256
p=argparse.ArgumentParser(description=__doc__);p.add_argument('stream',type=Path);p.add_argument('--splat-transform',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];folder=a.stream.resolve();source=folder/'chunk-0008.sog'
count=next(c['count'] for c in json.loads((folder/'scene.json').read_text())['chunks'] if c['file']==source.name)
output=folder/'comparison-sh0.ply';intermediate=folder/'comparison-intermediate.ply';provenance=output.with_suffix('.manifest.json')
if not output.exists():
 if not intermediate.exists():subprocess.run(['node',str(a.splat_transform),str(source),'-H','0',str(intermediate)],check=True)
 result=prepare(intermediate,output,count,'Huafa chunk-0008 SOG same-count comparison')
 result['source_sog_sha256']=sha256(source);result['output_bytes']=output.stat().st_size
 provenance.write_text(json.dumps(result,indent=2))
else:
 result=json.loads(provenance.read_text())
 if result.get('source_sog_sha256')!=sha256(source) or result['output_sha256']!=sha256(output):raise SystemExit('Existing comparison provenance does not match; preserve it and use a new output directory')
checkout=root/'.local/supersplat-viewer-reference'
revision='96f62515b99a28a20579041a656f7b1911c2964c'
if subprocess.check_output(['git','-C',str(checkout),'rev-parse','HEAD'],text=True).strip()!=revision:
 raise SystemExit('Unexpected upstream viewer revision')
# Bound the comparison backbuffer in the upstream resize function. Calling
# graphicsDevice.setResolution every update reallocates the canvas each frame.
index=checkout/'src/index.ts';text=index.read_text()
hook='''    const benchmarkSize = (window as unknown as { nextnewsResolution?: number[] }).nextnewsResolution;
    if (benchmarkSize) {
        if (canvas.width !== benchmarkSize[0]) canvas.width = benchmarkSize[0];
        if (canvas.height !== benchmarkSize[1]) canvas.height = benchmarkSize[1];
        return;
    }
'''
if hook not in text:
 anchor='    if (!cssSize.width || !cssSize.height) return;\n'
 if text.count(anchor)!=1:raise SystemExit('Upstream resize function changed; review the benchmark hook')
 index.write_text(text.replace(anchor,anchor+hook))
subprocess.run(['npm','run','build'],cwd=checkout,check=True)
reference=checkout/'public'
for name in ['index.js','index.css']:shutil.copyfile(reference/name,folder/name)
version=sha256(root/'scripts/harmonyos/web_stream_benchmark.js')[:12]
(folder/'web-compare.html').write_text((reference/'index.html').read_text().replace('</body>',f'<script src="web_stream_benchmark.js?v={version}"></script></body>'))
shutil.copyfile(root/'scripts/harmonyos/web_stream_benchmark.js',folder/'web_stream_benchmark.js')
shutil.copyfile(root/'apps/harmonyos/entry/src/main/resources/rawfile/viewer-settings.json',folder/'viewer-settings.json')
s=json.loads((folder/'viewer-settings.json').read_text());s['background']['color']=[.035,.045,.065];s['annotations']=[];s['highPrecisionRendering']=False;s['tonemapping']='none'
(folder/'web-settings.json').write_text(json.dumps(s))
print('Prepared same-count PLY and instrumented upstream viewer:',folder)
