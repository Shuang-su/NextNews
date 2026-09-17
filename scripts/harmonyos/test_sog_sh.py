"""Synthetic SOG SH1/2/3 fixtures; requires Pillow with lossless WebP support."""
import io,json,subprocess,tempfile,zipfile
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[2]
cmake='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/build-tools/cmake/bin/cmake'
build=root/'.local/sog-test-build'
subprocess.run([cmake,'-S',str(root/'apps/harmonyos/tests/sog'),'-B',str(build)],check=True)
subprocess.run([cmake,'--build',str(build),'--parallel','8'],check=True)
exe=build/'sog-test'
def webp(w,h,pixels):
 out=io.BytesIO();Image.frombytes('RGBA',(w,h),bytes(pixels)).save(out,format='WEBP',lossless=True,exact=True);return out.getvalue()
def fixture(n):
 m=dict(version=2,count=2,means=dict(mins=[0,0,0],maxs=[1,1,1],files=['l.webp','u.webp']),scales=dict(codebook=[-3]*256,files=['s.webp']),quats=dict(files=['q.webp']),sh0=dict(codebook=[0]*256,files=['c.webp']),shN=dict(codebook=[i/10 for i in range(256)],files=['h.webp','i.webp']))
 files={name:webp(2,1,[0,0,0,255]*2) for name in ['l.webp','u.webp','s.webp','c.webp']}
 files['q.webp']=webp(2,1,[128,128,128,252]*2)
 pixels=bytearray(64*n*2*4)
 for i,label in enumerate([0,65]):
  for k in range(n):
   at=((label//64)*64*n+(label%64)*n+k)*4
   pixels[at:at+4]=bytes([i*50+k*3+c for c in range(3)]+[255])
 files['h.webp']=webp(64*n,2,pixels);files['i.webp']=webp(2,1,[0,0,0,255,65,0,0,255])
 return m,files
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 def run(name,m,files,valid):
  path=d/(name+'.sog')
  with zipfile.ZipFile(path,'w') as z:
   z.writestr('meta.json',json.dumps(m));[z.writestr(k,v) for k,v in files.items()]
  result=subprocess.run([str(exe),str(path)]+(['--sh-fixture'] if valid else []),capture_output=True,text=True)
  assert result.returncode==(0 if valid else 1),(name,result.stdout,result.stderr)
  print(name,result.stdout.strip() if valid else result.stderr.strip())
  if valid:
   folder=d/name;folder.mkdir();(folder/'meta.json').write_text(json.dumps(m))
   for k,v in files.items():(folder/k).write_bytes(v)
   subprocess.run([str(exe),str(folder/'meta.json'),'--sh-fixture'],check=True)
 for degree,n in [(1,3),(2,8),(3,15)]:
  m,f=fixture(n);run('sh'+str(degree),m,f,True)
  bad=dict(f);bad['i.webp']=webp(2,1,[255,255,0,255]*2);run('label'+str(degree),m,bad,False)
 m,f=fixture(3);m['shN']['codebook']=[0];run('short-book',m,f,False)
 m,f=fixture(3);f['h.webp']=webp(191,2,[0,0,0,255]*382);run('width',m,f,False)
 m,f=fixture(3);del f['h.webp'];run('missing-centroids',m,f,False)
print('PASS bundled/unbundled SH degrees, independent coefficient addressing and malformed inputs')
