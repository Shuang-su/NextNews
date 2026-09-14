#!/usr/bin/env python3
"""Real phone check: render the old resident set while the next set is loading.
Requires project env.sh, a running local stream server and an installed debug HAP.
This checks responsiveness, not a sustained FPS benchmark.
"""
import argparse,csv,json,os,re,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--device');p.add_argument('--out',type=Path,default=Path('artifacts/harmonyos')/('loading-motion-'+time.strftime('%Y%m%d-%H%M%S')))
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
hdc=str(Path(os.environ.get('DEVECO_STUDIO_HOME','/Applications/DevEco-Studio.app'))/'Contents/sdk/default/openharmony/toolchains/hdc')
def run(cmd):return subprocess.run(cmd,check=True,capture_output=True,timeout=60).stdout.decode('utf-8','replace')
if not a.device:
 targets=run([hdc,'list','targets']).splitlines()
 if len(targets)!=1 or targets[0]=='[Empty]':raise SystemExit('Connect one target or specify --device')
 a.device=targets[0].strip()
def shell(s):return run([hdc,'-t',a.device,'shell',s])
def ui(*v):return run(['devecocli','ui',*v,'--device',a.device])
def nodes(ns):
 for n in ns:
  yield n
  yield from nodes(n.get('children',[]))
def layout():return json.loads(ui('layout','--format','json'))
def click(text):
 for attempt in range(3):
  n=next((n for n in nodes(layout()) if n.get('text')==text),None)
  if n:
   x,y,X,Y=n['bounds'];ui('click',str((x+X)//2),str((y+Y)//2));return
  ui('swipe','1000','2220','1000','1500','--speed','1000')
 raise RuntimeError('Control not visible: '+text)
shell('aa force-stop com.nextnews.splatviewer');shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(2)
pid=shell('pidof com.nextnews.splatviewer').strip().split()[0]
ui('click','--id','模型');click('流式');click('连接流式');ui('click','--id','模型')
for i in range(16):
 ui('drag','600','1000',str(680 if i%2==0 else 520),'1000','--speed','300')
 time.sleep(.25)
 if i in (1,8,15):
  (a.out/f'drag-{i}.json').write_text(json.dumps(layout(),ensure_ascii=False,indent=2))
  ui('screenshot','--path',str(a.out/f'drag-{i}.png'))
log=shell('hilog -x');rows=[]
for line in log.splitlines():
 parts=line.split()
 if len(parts)<4 or parts[2]!=pid:continue
 m=re.search(r'StreamFrame state=(\w+) frames=(\d+) count=(\d+) loadMs=([\d.]+) frameMs=([\d.]+)(?: uploadMs=([\d.]+))?',line)
 if m:rows.append([parts[1],m[1],int(m[2]),int(m[3]),float(m[4]),float(m[5]),float(m[6]) if m[6] else None])
with (a.out/'frames.csv').open('w') as f:
 writer=csv.writer(f);writer.writerow(['time','state','frames','resident_count','last_load_ms','last_draw_ms','last_upload_batch_ms']);writer.writerows(rows)
pairs=[(x,y) for x,y in zip(rows,rows[1:]) if x[1]==y[1]=='loading' and x[3]==y[3] and x[3]>500000 and y[2]>x[2]]
report={'loading_intervals_with_old_scene_drawing':len(pairs),'examples':pairs[-5:],'passes':len(pairs)>=2,'not_fps_benchmark':True}
(a.out/'result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,ensure_ascii=False))
if not report['passes']:raise SystemExit('No sufficient evidence of rendering during loading; inspect artifacts')
