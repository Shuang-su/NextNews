#!/usr/bin/env python3
"""Native voxel/walk invariants, malformed inputs, and optional upstream differential fixtures.
Use the per-command Xcode override documented in the HarmonyOS README.
"""
import json,math,os,struct,subprocess,tempfile,random
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];CORE=ROOT/'apps/harmonyos/entry/src/main/cpp';OUT=ROOT/'artifacts/harmonyos/collision-test';OUT.parent.mkdir(parents=True,exist_ok=True)
subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I',str(CORE),str(CORE/'collision/collision.cpp'),str(ROOT/'apps/harmonyos/tests/collision_test.cpp'),'-o',str(OUT)],check=True)
subprocess.run([str(OUT)],check=True)
def run(meta,binary,queries,valid=True):
 r=subprocess.run([str(OUT),str(meta),str(binary)],input=json.dumps(queries),text=True,capture_output=True,timeout=45)
 if valid:
  assert r.returncode==0,r.stderr
  return json.loads(r.stdout)
 assert r.returncode!=0,'Malformed file accepted'
 assert 'Sanitizer' not in r.stderr,r.stderr

def grid():
 # Dense 64^3 floor, wall and low ceiling; encode as sparse octree with mixed leaves.
 nodes=[0];leaves=[]
 def solid(x,y,z):return y==0 or (x==52 and y<30) or (y==14 and 8<=x<20 and 8<=z<20)
 def make(index,bx,by,bz,level):
  if level==0:
   mask=sum((1<<(z*16+y*4+x)) for z in range(4) for y in range(4) for x in range(4) if solid(bx*4+x,by*4+y,bz*4+z))
   nodes[index]=len(leaves)//2;leaves.extend([mask&0xffffffff,mask>>32]);return
  base=len(nodes);nodes.extend([0]*8);nodes[index]=(255<<24)|base
  for o in range(8):make(base+o,bx+((o&1)<<(level-1)),by+(((o>>1)&1)<<(level-1)),bz+(((o>>2)&1)<<(level-1)),level-1)
 make(0,0,0,0,4)
 return nodes,leaves

with tempfile.TemporaryDirectory(prefix='nextnews-collision-') as folder:
 folder=Path(folder);meta=folder/'test.voxel.json';binary=folder/'test.voxel.bin';nodes,leaves=grid()
 data={'version':'1.1','gridBounds':{'min':[-3.2,-.1,-3.2],'max':[3.2,6.3,3.2]},'voxelResolution':.1,'leafSize':4,'treeDepth':4,'nodeCount':len(nodes),'leafDataCount':len(leaves)}
 meta.write_text(json.dumps(data));binary.write_bytes(struct.pack('<'+'I'*(len(nodes)+len(leaves)),*(nodes+leaves)))
 cases=[{'op':'free','p':[0,1,0]},{'op':'free','p':[20,1,0]},{'op':'ray','p':[0,1.5,0],'d':[0,-1,0],'distance':3},{'op':'sphere','p':[0,.1,0],'radius':.2},{'op':'capsule','p':[1.9,1,0],'radius':.2,'half':.55},{'op':'spawn','p':[0,1.6,0]},
 {'op':'walk','p':[0,1.6,0],'frames':600}, {'op':'walk','p':[0,1.6,0],'frames':600,'right':4}, {'op':'walk','p':[0,1.6,0],'frames':600,'forward':4}, {'op':'walk','p':[0,1.6,0],'frames':600,'jump':True}]
 results=run(meta,binary,cases);assert results[0] is True and results[1] is False
 assert abs(results[2][1])<1e-7;assert abs(results[3][1]-.1)<1e-7;assert results[4][0]<-.099
 assert abs(results[5][1])<1e-7;assert abs(results[6]['eye'][1]-1.5)<1e-5
 assert results[7]['eye'][0]<=1.801 and results[7]['eye'][0]>1.7
 assert results[8]['blocked'] and results[8]['eye'][2]>-3
 assert 2<results[9]['peak']<2.5 and abs(results[9]['eye'][1]-1.5)<1e-5
 # Compare analytical geometry + random queries directly with the fixed upstream implementation.
 rng=random.Random(1337);queries=[]
 for i in range(250):
  p=[rng.uniform(-3,3),rng.uniform(.02,3),rng.uniform(-3,3)]
  if i%3==0:
   d=[rng.uniform(-1,1) for _ in range(3)];length=math.hypot(*d);queries.append({'op':'ray','p':p,'d':[v/length for v in d],'distance':10})
  else:queries.append({'op':'capsule' if i%3==1 else 'sphere','p':p,'radius':rng.uniform(.1,.4),'half':.55})
 native=run(meta,binary,queries)
 ref=ROOT/'.local/supersplat-viewer-reference/src/collision/voxel-collision.ts'
 if ref.exists():
  payload=folder/'queries.json';payload.write_text(json.dumps(queries));out=subprocess.run(['node',str(ROOT/'scripts/harmonyos/collision_reference.cjs'),str(meta),str(binary),str(payload)],text=True,capture_output=True,check=True)
  expected=json.loads(out.stdout)
  for i,(a,b) in enumerate(zip(native,expected)):
   assert (a is None)==(b is None),(i,queries[i],a,b)
   if a is not None:assert max(abs(x-y) for x,y in zip(a,b))<1e-6,(i,a,b)
  print('PASS 250 differential ray/sphere/capsule queries vs v1.31.2')
 else:print('SKIP differential: pinned upstream checkout unavailable')
 for mutate in [lambda d:d.update(leafSize=8),lambda d:d.update(treeDepth=1),lambda d:d.update(version='2.0'),lambda d:d.update(nodeCount=-1),lambda d:d.update(nodeCount=1.5),lambda d:d.update(treeDepth=4.5),lambda d:d.update(leafDataCount=2),lambda d:d.update(voxelResolution=0),lambda d:d['gridBounds']['min'].__setitem__(0,float('nan'))]:
  bad=json.loads(json.dumps(data));mutate(bad);meta.write_text(json.dumps(bad));run(meta,binary,[],False)
 meta.write_text(json.dumps(data));raw=binary.read_bytes()
 for marker in [0x01000000,0x01ffffff,0x00ffffff]:
  binary.write_bytes(struct.pack('<I',marker)+raw[4:]);run(meta,binary,[],False)
 binary.write_bytes(raw[:-1]);run(meta,binary,[],False)
 binary.write_bytes(raw);legacy=dict(data,version='1.0');meta.write_text(json.dumps(legacy))
 legacyHit=run(meta,binary,[{'op':'ray','p':[0,-1.5,0],'d':[0,1,0],'distance':3}])[0];assert abs(legacyHit[1])<1e-7
 meta.write_text(json.dumps(dict(data,nodeCount=0,leafDataCount=0)));binary.write_bytes(b'');assert run(meta,binary,[{'op':'free','p':[0,1,0]}])==[False]
print('PASS native collision: stable floor, wall, jump/release latch, unknown boundary stop, reset, legacy transform and malformed data (ASan + UBSan)')
