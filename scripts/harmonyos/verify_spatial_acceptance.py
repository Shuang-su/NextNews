#!/usr/bin/env python3
"""Historical evidence harness (Huawei research discontinued by user 2026-09-18).
Capture same-source Huawei GSNode and TiledGSNode evidence on the paired phone.
Source env.sh. Requires the verified Huafa comparison files served on port 8768.
Screenshots and callback logs require review; API success is never a visual pass.
"""
import argparse,json,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
hdc='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc'
def run(args):return subprocess.run(args,check=True,capture_output=True,timeout=60).stdout.decode('utf8','replace')
devices=run([hdc,'list','targets']).splitlines();assert len(devices)==1 and devices[0]!='[Empty]';device=devices[0]
def shell(s):return run([hdc,'-t',device,'shell',s])
def ui(*v):return run(['devecocli','ui',*v,'--device',device])
def nodes(ns):
 for n in ns:
  yield n
  yield from nodes(n.get('children',[]))
def layout():return json.loads(ui('layout','--format','json'))
def tap(label,key='text'):
 for _ in range(9):
  n=next((n for n in nodes(layout()) if n.get(key)==label and n['bounds'][2]>n['bounds'][0] and n['bounds'][3]>n['bounds'][1]),None)
  if n:
   x,y,X,Y=n['bounds'];ui('click',str((x+X)//2),str((y+Y)//2));time.sleep(.5);return
  ui('swipe','1000','2220','1000','1500','--speed','1000')
 raise RuntimeError('Missing visible control '+label)
def snap(name):
 data=layout();(a.out/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2));ui('screenshot','--path',str(a.out/(name+'.png')))
shell('aa force-stop com.nextnews.splatviewer');shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(2)
pid=shell('pidof com.nextnews.splatviewer').strip().split()[0]
(a.out/'device.txt').write_text(shell('param get const.product.model')+shell('param get const.ohos.apiversion')+shell('param get const.product.software.version'))
record=(a.out/'hilog.txt').open('wb');logger=subprocess.Popen([hdc,'-t',device,'shell','hilog -P '+pid],stdout=record,stderr=subprocess.STDOUT)
try:
 tap('模型','id');tap('同源 SOG 对照');time.sleep(20);snap('opengl-sog')
 tap('模型','id');tap('华为瓦片实验');time.sleep(3);snap('huawei-public')
 assert any('public.ply' in n.get('text','') for n in nodes(layout())), 'Public button must load public.ply, independent of fixture array order'
 for label,name in [('同源 SOG · loadGSNode','huawei-sog'),('同源 PLY','huawei-ply')]:
  tap(label);time.sleep(5);tap('配置默认镜头');time.sleep(1);snap(name)
 shell('uitest uiInput keyEvent Home');time.sleep(1);shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(4);snap('huawei-ply-resumed')
 tap('连接原生流式');time.sleep(12);snap('huawei-tiled-initial')
 ui('swipe','700','1100','1000','1200','--speed','500');time.sleep(4);snap('huawei-tiled-moved')
 tap('公开样例');time.sleep(3);snap('huawei-after-tiled-cancel')
 (a.out/'memory.txt').write_text(shell('hidumper --mem '+pid))
finally:
 logger.terminate()
 try:logger.wait(timeout=5)
 except subprocess.TimeoutExpired:logger.kill();logger.wait()
 record.close()
log=(a.out/'hilog.txt').read_text(errors='replace')
(a.out/'result.json').write_text(json.dumps({'scope':'same-source 524598 SOG / SH0 PLY; separate tiled probe','tileReadyCallbacks':log.count('SpatialStream tile ready'),'visualReviewRequired':True,'performanceAcceptance':False},indent=2))
print('Native captures collected. Review visible geometry, colors, resume and actual tiled callbacks.')
