"""Original direction-color fixture, SOG v2. Pillow lossless WebP required."""
import argparse,io,json,math,zipfile
from pathlib import Path
from PIL import Image
parser=argparse.ArgumentParser();parser.add_argument('output',type=Path);parser.add_argument('--degree',type=int,choices=[1,2,3],default=1);args=parser.parse_args()
def webp(w,h,data):
 f=io.BytesIO();Image.frombytes('RGBA',(w,h),bytes(data)).save(f,format='WEBP',lossless=True,exact=True);return f.getvalue()
n={1:3,2:8,3:15}[args.degree];bound=math.log1p(.525)
meta=dict(version=2,count=256,comment='NextNews original direction-color fixture',means=dict(mins=[-bound,-bound,0],maxs=[bound,bound,0],files=['l.webp','u.webp']),scales=dict(codebook=[math.log(.045)]*256,files=['s.webp']),quats=dict(files=['q.webp']),sh0=dict(codebook=[0]*256,files=['c.webp']),shN=dict(codebook=[0,1,-1]+[0]*253,files=['h.webp','i.webp']))
low=[];high=[]
for y in range(16):
 for x in range(16):
  q=[]
  for value in [(x-7.5)*.07,(y-7.5)*.07]:
   log=math.copysign(math.log1p(abs(value)),value);q.append(round((log+bound)/(2*bound)*65535))
  low.extend([q[0]&255,q[1]&255,0,255]);high.extend([q[0]>>8,q[1]>>8,0,255])
centroids=bytearray([0,0,0,255]*64*n);centroids[4:8]=bytes([1,0,2,255])
files={'l.webp':webp(16,16,low),'u.webp':webp(16,16,high),'s.webp':webp(16,16,[0,0,0,255]*256),'q.webp':webp(16,16,[128,128,128,252]*256),'c.webp':webp(16,16,[0,0,0,243]*256),'h.webp':webp(64*n,1,centroids),'i.webp':webp(16,16,[0,0,0,255]*256)}
with zipfile.ZipFile(args.output,'x') as z:
 z.writestr('meta.json',json.dumps(meta))
 for name,data in files.items():z.writestr(name,data)
