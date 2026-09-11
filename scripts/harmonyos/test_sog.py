#!/usr/bin/env python3
"""Build the host decoder with ASan/UBSan; compare to an independent SH0 PLY and reject bad archives."""
import argparse,json,subprocess,tempfile,zipfile,struct
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('sog',type=Path);p.add_argument('reference',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];cmake='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/build-tools/cmake/bin/cmake';build=root/'.local/sog-test-build'
subprocess.run([cmake,'-S',str(root/'apps/harmonyos/tests/sog'),'-B',str(build)],check=True)
subprocess.run([cmake,'--build',str(build),'--parallel','8'],check=True)
exe=build/'sog-test';subprocess.run([str(exe),str(a.sog.resolve()),str(a.reference.resolve())],check=True)
with zipfile.ZipFile(a.sog) as z:files={n:z.read(n) for n in z.namelist()}
with tempfile.TemporaryDirectory(prefix='nextnews-sog-') as tmp:
 def check(name,data):
  path=Path(tmp)/(name+'.sog');path.write_bytes(data);r=subprocess.run([str(exe),str(path)],capture_output=True,text=True)
  if r.returncode!=1:raise AssertionError((name,r.returncode,r.stdout,r.stderr))
  print('PASS reject',name,r.stderr.strip())
 def archive(name,content):
  path=Path(tmp)/(name+'.zip')
  with zipfile.ZipFile(path,'w') as z:
   for k,v in content.items():z.writestr(k,v)
  return path.read_bytes()
 check('truncated',a.sog.read_bytes()[:-22])
 corrupt=bytearray(a.sog.read_bytes());corrupt[100]^=1;check('checksum',corrupt)
 for name,change in [('count',lambda m:m.update(count=4000001)),('shape',lambda m:m['scales'].update(codebook=[0])),('scale',lambda m:m['scales']['codebook'].__setitem__(0,1000)),('texture_count',lambda m:m.update(count=2000000)),('nesting',lambda m:m.update(extra=json.loads('['*40+'0'+']'*40)))]:
  m=json.loads(files['meta.json']);change(m);content=dict(files);content['meta.json']=json.dumps(m).encode();check(name,archive(name,content))
 content=dict(files);content['../escape']=b'bad';check('traversal',archive('traversal',content))
print('PASS native SOG reference comparison and invalid archive suite')
