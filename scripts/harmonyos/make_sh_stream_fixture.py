"""Create three original SOG sources with independent SH books for page tests."""
import argparse,json,math,subprocess,sys,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);args=p.parse_args();args.directory.mkdir(parents=True,exist_ok=False)
chunks=[]
for degree in [1,2,3]:
 path=args.directory/f'chunk-{degree:04d}.sog'
 subprocess.run([sys.executable,str(Path(__file__).with_name('make_sog_sh_fixture.py')),str(path),'--degree',str(degree)],check=True)
 with zipfile.ZipFile(path) as z:files={n:z.read(n) for n in z.namelist()}
 meta=json.loads(files['meta.json']);center=(degree-2)*1.4
 def log(x):return math.copysign(math.log1p(abs(x)),x)
 meta['means']['mins'][0]=log(center-.525);meta['means']['maxs'][0]=log(center+.525)
 if degree==2:meta['shN']['codebook'][1:3]=[-1,1]
 files['meta.json']=json.dumps(meta).encode()
 with zipfile.ZipFile(path,'w') as z:
  for name,data in files.items():z.writestr(name,data)
 chunks.append(dict(file=path.name,count=256,bytes=path.stat().st_size,bounds=[center,0,0,1]))
(args.directory/'scene.json').write_text(json.dumps(dict(version=1,bounds=[0,0,0,2.1],chunks=chunks),indent=2))
(args.directory/'provenance.json').write_text(json.dumps(dict(source='NextNews original synthetic planes',purpose='Independent source SH1/2/3 books and stable-page residency; nonzero first-order terms only'),indent=2))
