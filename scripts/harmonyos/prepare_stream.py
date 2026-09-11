#!/usr/bin/env python3
"""Spatially split decoded float32 3DGS PLY into bounded, independently valid PLY chunks."""
import argparse, json, math, struct
from pathlib import Path
from prepare_model import FIELDS, sha256

def split(source, destination):
    source=Path(source); destination=Path(destination)
    if destination.exists(): raise ValueError('Output directory already exists')
    with source.open('rb') as f:
        if f.readline().strip()!=b'ply': raise ValueError('Not PLY')
        props=[]; count=None; valid_format=False
        for _ in range(512):
            line=f.readline(8192).decode('ascii').strip(); words=line.split()
            if line=='end_header': break
            if words[:1]==['format']:
                if valid_format or words[1:]!=['binary_little_endian','1.0']: raise ValueError('Invalid format')
                valid_format=True
            if words[:1]==['element']:
                if words[1]!='vertex' or count is not None: raise ValueError('Vertex-only required')
                count=int(words[2])
            if words[:1]==['property']:
                if len(words)!=3 or words[1]!='float' or words[2] in props: raise ValueError('Unique float fields required')
                props.append(words[2])
        else: raise ValueError('Missing end_header')
        if not valid_format or not count or count>20_000_000: raise ValueError('Invalid vertex count')
        fmt=struct.Struct('<'+'f'*len(props)); indices=[props.index(x) for x in FIELDS]
        payload=f.read(count*fmt.size+1)
        if len(payload)!=count*fmt.size: raise ValueError('Payload size mismatch')
    rows=[]; endpoints=0
    for raw in fmt.iter_unpack(payload):
        v=[raw[i] for i in indices]
        if math.isinf(v[6]): v[6]=math.copysign(20,v[6]); endpoints+=1
        if not all(math.isfinite(x) for x in v): raise ValueError('Non-finite Gaussian')
        if any(abs(x)>1e6 for x in v[:3]) or any(x < -30 or x>14 for x in v[7:10]): raise ValueError('Position/scale outside bounds')
        n=math.sqrt(sum(x*x for x in v[10:]));
        if n<1e-6: raise ValueError('Zero quaternion')
        v[10:]=[x/n for x in v[10:]];rows.append(v)
    def bounds(rows):
        lo=[min(r[k] for r in rows) for k in range(3)]; hi=[max(r[k] for r in rows) for k in range(3)]
        return [(lo[k]+hi[k])/2 for k in range(3)]+[max(.001,math.dist(lo,hi)/2)]
    groups=[]
    def partition(rows):
        if len(rows)<=32768: groups.append(rows); return
        axis=max(range(3),key=lambda k:max(r[k] for r in rows)-min(r[k] for r in rows))
        rows.sort(key=lambda r:r[axis]); mid=len(rows)//2
        partition(rows[:mid]);partition(rows[mid:])
    global_bounds=bounds(rows);partition(rows)
    if len(groups)>1024: raise ValueError('Too many chunks')
    destination.mkdir(parents=True)
    chunks=[]
    for i,group in enumerate(groups):
        name=f'chunk-{i:04}.ply'; path=destination/name
        header='ply\nformat binary_little_endian 1.0\nelement vertex '+str(len(group))+'\n'+''.join('property float '+p+'\n' for p in FIELDS)+'end_header\n'
        with path.open('xb') as f:
            f.write(header.encode()); f.write(b''.join(struct.pack('<14f',*r) for r in group))
        chunks.append(dict(file=name,count=len(group),bytes=path.stat().st_size,bounds=bounds(group),sha256=sha256(path)))
    manifest=dict(version=1,bounds=global_bounds,chunks=chunks)
    (destination/'scene.json').write_text(json.dumps(manifest,indent=2))
    (destination/'provenance.json').write_text(json.dumps(dict(source=str(source.resolve()),source_sha256=sha256(source),count=count,sh_degree=0,high_order_sh_removed=any(p.startswith('f_rest_') for p in props),opacity_endpoints=endpoints,partition='recursive longest-axis median; no decimation',license='Private local development input; preserve source provenance'),indent=2))
    print(f'{count} Gaussians -> {len(chunks)} chunks at {destination}')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('destination');a=p.parse_args();split(a.source,a.destination)
