#!/usr/bin/env python3
"""Generate a tiny original SH0 fixture; no external or user model is read.
Use a new output directory. PLY receives normal prepare_model provenance.
World-space labels deliberately straddle three red Gaussian occluders.
"""
import argparse, json, math, struct
from pathlib import Path
from prepare_model import FIELDS, prepare
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args(); a.out.mkdir(parents=True, exist_ok=False)
rows = []
for x,y,sx,sy in [(-.8,0,.5,.5),(.8,0,.5,.5),(.04,1,.06,.3)]:
    # PLY is model space, renderer imports with Rz(180). Labels remain world space.
    rows.extend([-x,-y,0,*[(v-.5)/.28209479177387814 for v in [.9,.08,.03]],
                 math.log(99),math.log(sx),math.log(sy),math.log(.01),1,0,0,0])
source = a.out/'annotation-source.ply'
source.write_bytes(('ply\nformat binary_little_endian 1.0\nelement vertex 3\n'+
    ''.join('property float '+f+'\n' for f in FIELDS)+'end_header\n').encode()+struct.pack('<42f',*rows))
destination = a.out/'annotation-fixture.ply'
manifest = prepare(source,destination,label='NextNews original annotation depth fixture',license_note='MIT — NextNews original analytic fixture')
manifest['source']='scripts/harmonyos/make_annotation_fixture.py: generated annotation-source.ply'
destination.with_suffix('.manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
pose={'position':[0,0,6],'target':[0,0,0],'fov':60}
names=['前景标注','后景标注','半透明遮挡','镜头后方','屏幕外浮窗']
positions=[[-.8,0,1],[.8,0,-1],[0,1,-1],[0,0,7],[100,20,0]]
data={'version':2,'cameras':[{'initial':pose}],'animTracks':[],'startMode':'default',
      'tonemapping':'none','background':{'color':[.035,.045,.065]},'annotations':[
          {'position':point,'title':name,'text':'编号 '+str(i+1)+'。<b>保持纯文本</b>。遮挡后仍可点击。',
           'camera':{'initial':pose}} for i,(point,name) in enumerate(zip(positions,names))]}
(a.out/'annotation-fixture.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
print('Created original 3-Gaussian / 5-annotation fixture:',a.out)
