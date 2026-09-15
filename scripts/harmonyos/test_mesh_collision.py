#!/usr/bin/env python3
"""Bounded GLB parsing and collision differential tests against Viewer v1.31.2."""
import json,struct,subprocess,random,math,tempfile,copy,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];CORE=ROOT/'apps/harmonyos/entry/src/main/cpp';OUT=ROOT/'artifacts/harmonyos/mesh-test';OUT.parent.mkdir(exist_ok=True,parents=True)
subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I',str(CORE),str(CORE/'collision/collision.cpp'),str(CORE/'collision/mesh.cpp'),str(ROOT/'apps/harmonyos/tests/mesh_test.cpp'),'-o',str(OUT)],check=True)
def glb(doc,bin):
 text=json.dumps(doc,separators=(',',':')).encode();text+=b' '*((-len(text))%4);bin+=b'\0'*((-len(bin))%4)
 body=struct.pack('<II',len(text),0x4e4f534a)+text+struct.pack('<II',len(bin),0x004e4942)+bin
 return struct.pack('<III',0x46546c67,2,len(body)+12)+body
def run(path,queries,valid=True):
 r=subprocess.run([str(OUT),str(path)],input=json.dumps(queries),text=True,capture_output=True,timeout=45)
 if valid:
  assert r.returncode==0,r.stderr
  return json.loads(r.stdout)
 assert r.returncode!=0,'Malformed GLB accepted'
 assert 'Sanitizer' not in r.stderr,r.stderr
with tempfile.TemporaryDirectory(prefix='nextnews-mesh-') as tmp:
 tmp=Path(tmp);path=tmp/'room.glb'
 # Floor, vertical wall, sloped ramp. Identity GLB mesh resources are baked world coordinates.
 pos=[[-4,0,-4],[-4,0,4],[4,0,4],[4,0,-4],[2,0,-4],[2,3,-4],[2,3,4],[2,0,4],[-3,0,0],[-3,0,3],[-1,1,3],[-1,1,0]]
 idx=[0,1,2,0,2,3,4,5,6,4,6,7,8,9,10,8,10,11]
 vertices=struct.pack('<'+'f'*(len(pos)*3),*(n for p in pos for n in p));binary=vertices+struct.pack('<'+'H'*len(idx),*idx)
 doc={'asset':{'version':'2.0'},'buffers':[{'byteLength':len(binary)}],'bufferViews':[{'buffer':0,'byteOffset':0,'byteLength':len(vertices)},{'buffer':0,'byteOffset':len(vertices),'byteLength':len(idx)*2}],'accessors':[{'bufferView':0,'componentType':5126,'count':len(pos),'type':'VEC3'},{'bufferView':1,'componentType':5123,'count':len(idx),'type':'SCALAR'}],'meshes':[{'primitives':[{'attributes':{'POSITION':0},'indices':1}]}],'nodes':[{'mesh':0}],'scenes':[{'nodes':[0]}],'scene':0}
 path.write_bytes(glb(doc,binary))
 cases=[{'op':'ray','p':[0,2,0],'d':[0,-1,0],'distance':10},{'op':'sphere','p':[0,.1,0],'radius':.2},{'op':'capsule','p':[1.9,1,0],'radius':.2,'half':.55},{'op':'spawn','p':[0,1.6,0]},{'op':'walk','p':[0,1.6,0],'frames':600,'right':4},{'op':'walk','p':[0,1.6,0],'frames':600,'jump':True}]
 values=run(path,cases);assert abs(values[0][1])<1e-8 and abs(values[1][1]-.1)<1e-8 and values[2][0]<-.099
 assert abs(values[3][1])<1e-8 and 1.7<values[4]['eye'][0]<1.801 and 2<values[5]['peak']<2.5 and abs(values[5]['eye'][1]-1.5)<1e-5
 rng=random.Random(24680);queries=[]
 for i in range(300):
  p=[rng.uniform(-3.5,3.5),rng.uniform(.02,3),rng.uniform(-3.5,3.5)]
  if i%3==0:queries.append({'op':'ray','p':p,'d':[rng.uniform(-1,1) for _ in range(3)],'distance':12})
  else:queries.append({'op':'sphere' if i%3==1 else 'capsule','p':p,'radius':rng.uniform(.1,.4),'half':.55})
 payload=tmp/'geometry.json';payload.write_text(json.dumps({'positions':pos,'indices':idx}));qfile=tmp/'queries.json';qfile.write_text(json.dumps(queries))
 reference=json.loads(subprocess.check_output(['node',str(ROOT/'scripts/harmonyos/mesh_reference.cjs'),str(payload),str(qfile)],text=True));actual=run(path,queries)
 for i,(a,b) in enumerate(zip(actual,reference)):
  assert (a is None)==(b is None),(i,queries[i],a,b)
  if a is not None:assert max(abs(x-y) for x,y in zip(a,b))<1e-5,(i,queries[i],a,b)
 # The fixed official factory uses mesh resource positions and ignores node transforms.
 transformed=copy.deepcopy(doc);transformed['nodes'][0]['translation']=[100,20,-4];path.write_bytes(glb(transformed,binary));assert run(path,cases)==values
 for change in [lambda d:d['accessors'][0].update(count=1000000),lambda d:d['accessors'][0].update(count=12.5),lambda d:d['accessors'][0].update(sparse={}),lambda d:d['accessors'][0].update(componentType=5123),lambda d:d['accessors'][1].update(count=17),lambda d:d['bufferViews'][0].update(byteStride=5),lambda d:d['buffers'][0].update(uri='https://invalid.test/a.bin'),lambda d:d['meshes'][0]['primitives'][0].update(mode=1),lambda d:d['meshes'][0]['primitives'][0].update(mode=4.5),lambda d:d.update(extensionsRequired=['KHR_draco_mesh_compression'])]:
  bad=copy.deepcopy(doc);change(bad);path.write_bytes(glb(bad,binary));run(path,[],False)
 for b in [struct.pack('<f',float('nan'))+binary[4:],vertices+struct.pack('<H',65000)+binary[len(vertices)+2:]]:
  path.write_bytes(glb(doc,b));run(path,[],False)
 nested=copy.deepcopy(doc);n={};nested['extras']=n
 for _ in range(100):n['deep']={};n=n['deep']
 path.write_bytes(glb(nested,binary));run(path,[],False)
 full=glb(doc,binary)
 for b in [full[:-1],full[:20],b'xxxx'+full[4:],full[:8]+struct.pack('<I',2)+full[12:]]:path.write_bytes(b);run(path,[],False)
print('PASS mesh floor/wall/spawn/jump; 300 upstream ray/sphere/capsule comparisons; GLB bounds, indices, extensions, nonfinite and truncation guards (ASan+UBSan)')
