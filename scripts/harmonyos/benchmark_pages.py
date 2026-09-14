#!/usr/bin/env python3
"""Run the app's 20-pose, full-target-coverage page benchmark on one real device.
Requires env.sh, installed debug HAP and the stream server on 8768.
Captures polling-based end-to-end upper bounds separately from native swap timing.
"""
import argparse,csv,json,math,os,re,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--file-only',action='store_true');p.add_argument('--device');p.add_argument('--budget',type=int,choices=[2000000,4000000,8000000],default=2000000)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
hdc='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc'
def run(args):return subprocess.run(args,check=True,capture_output=True,timeout=60).stdout.decode('utf8','replace')
if not a.device:
 devices=run([hdc,'list','targets']).splitlines()
 if len(devices)!=1 or devices[0]=='[Empty]':raise SystemExit('Connect one device or specify --device')
 a.device=devices[0]
def shell(s):return run([hdc,'-t',a.device,'shell',s])
def ui(*v):
 result=run(['devecocli','ui',*v,'--device',a.device])
 if v[0] not in ('layout','screenshot'):time.sleep(.5)
 return result
def nodes(ns):
 for n in ns:
  yield n
  yield from nodes(n.get('children',[]))
def layout():return json.loads(ui('layout','--format','json'))
def click(text):
 for _ in range(4):
  n=next((n for n in nodes(layout()) if n.get('text')==text),None)
  if n:
   x,y,X,Y=n['bounds'];ui('click',str((x+X)//2),str((y+Y)//2));return
  ui('swipe','1000','2220','1000','1500','--speed','1000')
 raise RuntimeError('Missing control: '+text)
shell('aa force-stop com.nextnews.splatviewer');shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(2)
pid=shell('pidof com.nextnews.splatviewer').strip().split()[0]
ui('click','--id','设置');click('流式：兼容')
if a.budget>=4000000:click('200 万')
if a.budget>=8000000:click('400 万')
ui('click','--id','设置');ui('click','--id','模型');click('流式');click('连接流式');ui('click','--id','模型')
ui('click','--id','设置');click('性能信息');click('文件缓存测量 20 次' if a.file_only else '流式测量 20 次');ui('click','--id','设置')
start=time.monotonic();lines=[]
while time.monotonic()-start<600:
 raw=shell('hilog -x')
 lines=[l for l in raw.splitlines() if len(l.split())>2 and l.split()[2]==pid]
 if any('StreamTrial failure=' in l for l in lines):break
 if any('StreamTrial trial=19 ' in l for l in lines):break
 time.sleep(3)
(a.out/'hilog.txt').write_text('\n'.join(lines))
(a.out/'layout.json').write_text(json.dumps(layout(),ensure_ascii=False,indent=2))
ui('screenshot','--path',str(a.out/'final.png'))
rows=[]
for l in lines:
 if 'StreamTrial trial=' in l:
  d={k:float(v) for k,v in re.findall(r'(\w+)=([-\d.]+)',l)}
  if d['trial']>=0:rows.append(d)
if rows:
 with (a.out/'trials.csv').open('w') as f:
  w=csv.DictWriter(f,fieldnames=rows[0],lineterminator='\n');w.writeheader();w.writerows(rows)
def percentile(key,q):
 return sorted(r[key] for r in rows)[max(0,math.ceil(len(rows)*q)-1)] if rows else None
result={'cache':'file' if a.file_only else 'gpu','budget':a.budget,'trials':len(rows),'endToEndP50Ms':percentile('elapsedMs',.5),'endToEndP95Ms':percentile('elapsedMs',.95),
 'nativeSubmitToSwapP95Ms':percentile('refineMs',.95),'allUploadsZero':bool(rows) and all(r['uploadedBytes']==0 for r in rows),
 'fileOnlyFreshTargetPages':a.file_only and bool(rows) and all(r['uploadedBytes']>=r['count']*64 for r in rows),
 'fullCoverage':bool(rows) and all(r['coverage']==1 for r in rows),'frameTimeGateTested':False,
 'failure':[l for l in lines if 'StreamTrial failure=' in l]}
(a.out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2),flush=True)
if len(rows)!=20 or result['failure']:raise SystemExit('Benchmark incomplete; inspect artifacts')
