#!/usr/bin/env python3
"""Measure the pinned SuperSplat reference in the app's ArkWeb on one phone.
Requires env.sh, prepare_comparison.py, server 8768 and HDC reverse port.
These are upstream readiness results, not NextNews target-coverage gates.
"""
import argparse,json,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--budget',type=int,choices=[2,4,8],default=2)
p.add_argument('--device');p.add_argument('--out',type=Path,required=True)
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
def click(text):
 tree=json.loads(ui('layout','--format','json'))
 n=next(n for n in nodes(tree) if n.get('text')==text)
 x,y,X,Y=n['bounds'];ui('click',str((x+X)//2),str((y+Y)//2))
shell('aa force-stop com.nextnews.splatviewer');shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(2)
pid=shell('pidof com.nextnews.splatviewer').strip().split()[0]
ui('click','--id','模型');click('SuperSplat Web 对照');time.sleep(3)
if a.budget>=4:click('200 万 Web')
if a.budget>=8:click('400 万 Web')
time.sleep(8);click('Web 测量')
start=time.monotonic();lines=[];captured={}
while time.monotonic()-start<600:
 captured.update(dict.fromkeys(l for l in shell('hilog -x').splitlines() if len(l.split())>2 and l.split()[2]==pid and '/JSAPP:' in l))
 lines=list(captured)
 if any('WebBenchmark ' in l for l in lines):break
 time.sleep(3)
(a.out/'hilog.txt').write_text('\n'.join(lines))
ui('screenshot','--path',str(a.out/'final.png'))
(a.out/'memory.txt').write_text(shell('hidumper --mem '+pid))
results=[json.loads(l.split('WebBenchmark ',1)[1]) for l in lines if 'WebBenchmark {' in l]
trials=[json.loads(l.split('WebTrial ',1)[1]) for l in lines if 'WebTrial {' in l]
(a.out/'trials.json').write_text(json.dumps(trials,indent=2))
if not results:raise SystemExit('Web benchmark incomplete; inspect hilog.txt')
result=results[-1];result['cacheCondition']='two warm-up poses; upstream browser/CPU/GPU caches not separated'
(a.out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2),flush=True)
if result['trials']!=20 or sum(r['trial']>=0 for r in trials)!=20 or result['width']!=1320 or result['height']!=2623:raise SystemExit('Web comparison conditions or individual trial records not met')
