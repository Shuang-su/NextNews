#!/usr/bin/env python3
"""Bundle an existing SuperSplat lod-meta tree without resampling or changing source data."""
import argparse,json,math,zipfile,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
meta=json.loads(a.source.read_text());root=a.source.parent
if a.output.exists():raise SystemExit('Output must be a new directory')
a.output.mkdir(parents=True)
def bounds(b):
 lo=b['min'];hi=b['max'];return [(lo[k]+hi[k])/2 for k in range(3)]+[max(.001,math.dist(lo,hi)/2)]
files=[]
for index,name in enumerate(meta['filenames'] + ([meta['environment']] if meta.get('environment') else [])):
 f=root/name;m=json.loads(f.read_text());out=a.output/f'chunk-{index:04d}.sog'
 with zipfile.ZipFile(out,'w',compression=zipfile.ZIP_STORED) as z:
  m.pop('shN',None)
  z.writestr('meta.json',json.dumps(m,separators=(',',':')))
  for field in ('means','quats','scales','sh0'):
   for image in m[field]['files']:
    if Path(image).name!=image:raise ValueError('Texture filename must be a basename')
    z.write(f.parent/image,image)
 lo=[math.copysign(math.expm1(abs(v)),v) for v in m['means']['mins']];hi=[math.copysign(math.expm1(abs(v)),v) for v in m['means']['maxs']]
 files.append(dict(sha256=hashlib.sha256(out.read_bytes()).hexdigest(),file=out.name,count=m['count'],bytes=out.stat().st_size,bounds=bounds(dict(min=lo,max=hi))))
leaves=[]
def walk(node):
 if 'lods' in node:
  lods=[]
  for level in range(meta['lodLevels']):
   item=node['lods'].get(str(level));lods.append([item['file'],item['offset'],item['count']] if item else [-1,0,0])
  leaves.append(dict(bounds=bounds(node['bound']),aabb=node['bound']['min']+node['bound']['max'],lods=lods))
 for child in node.get('children',[]):walk(child)
walk(meta['tree'])
if meta.get('environment'):
 index=len(files)-1;leaves.append(dict(bounds=files[index]['bounds'],lods=[[index,0,files[index]['count']] for _ in range(meta['lodLevels'])]))
manifest=dict(version=2,bounds=bounds(meta['tree']['bound']),chunks=files,leaves=leaves,levels=meta['lodLevels'])
(a.output/'scene.json').write_text(json.dumps(manifest,separators=(',',':')))
web=dict(meta);web['filenames']=[c['file'] for c in files[:len(meta['filenames'])]]
if meta.get('environment'):web['environment']=files[-1]['file']
(a.output/'lod-meta.json').write_text(json.dumps(web,separators=(',',':')))
(a.output/'provenance.json').write_text(json.dumps(dict(source=str(a.source),sha256=hashlib.sha256(a.source.read_bytes()).hexdigest(),sourceCounts=meta['counts'],gaussiansRemoved=0,color='SH0; higher SH textures omitted',environment='included as always-resident leaf',sourceUnchanged=True),indent=2))
print(json.dumps(dict(files=len(files),leaves=len(leaves),bytes=sum(f['bytes'] for f in files))))
