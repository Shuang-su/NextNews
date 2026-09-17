#!/usr/bin/env python3
"""Real phone annotation fixture evidence; source env.sh. Review PNGs separately.
This is a functional depth/UI test, not SuperSplat pixel or performance parity.
"""
import argparse,json,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--include-huawei',action='store_true',help='Historical opt-in only; Huawei research was discontinued');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
hdc='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc'
def run(args):return subprocess.run(args,check=True,capture_output=True,timeout=60).stdout.decode('utf8','replace')
devices=run([hdc,'list','targets']).splitlines();assert len(devices)==1 and devices[0]!='[Empty]';device=devices[0]
def shell(text):return run([hdc,'-t',device,'shell',text])
def ui(*args):return run(['devecocli','ui',*args,'--device',device])
def nodes(tree):
 for n in tree:
  yield n
  yield from nodes(n.get('children',[]))
def layout():return json.loads(ui('layout','--format','json'))
def icon(label):
 # Resolve the current accessibility tree: CLI --id can miss a present node after resume.
 n=next((n for n in nodes(layout()) if n.get('id')==label and
         n['bounds'][2]>n['bounds'][0] and n['bounds'][3]>n['bounds'][1]),None)
 if n is None:raise RuntimeError('Missing icon '+label)
 x,y,X,Y=n['bounds'];ui('click',str((x+X)//2),str((y+Y)//2));time.sleep(.4)
def click(label):
 for i in range(7):
  n=next((n for n in nodes(layout()) if n.get('text')==label and n['bounds'][2]>n['bounds'][0] and n['bounds'][3]>n['bounds'][1]),None)
  if n:
   x,y,X,Y=n['bounds'];ui('click',str((x+X)//2),str((y+Y)//2));time.sleep(.5);return
  ui('swipe','1000','2200','1000','1530','--speed','1000')
 snap('missing-control');raise RuntimeError('Missing '+label)
def snap(name):
 data=layout();(a.out/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2));ui('screenshot','--path',str(a.out/(name+'.png')));return data
def contains(data,id):return any(n.get('id')==id for n in nodes(data))
shell('aa force-stop com.nextnews.splatviewer');shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(2)
pid=shell('pidof com.nextnews.splatviewer').strip().split()[0]
(a.out/'device.txt').write_text(shell('param get const.product.model')+shell('param get const.ohos.apiversion')+shell('param get const.product.software.version'))
icon('设置');click('性能信息');click('标注遮挡夹具');time.sleep(1)
first=snap('fixture-opengl');assert all(contains(first,'annotation-'+str(i)) for i in range(3));assert not contains(first,'annotation-3');assert not contains(first,'annotation-4')
icon('annotation-1');time.sleep(.6);selected=snap('popup-occluded-opengl');assert contains(selected,'annotation-popup');assert any(n.get('text')=='编号 2。<b>保持纯文本</b>。遮挡后仍可点击。' for n in nodes(selected))
icon('关闭标注');assert not contains(layout(),'annotation-popup')
icon('下一个标注');time.sleep(.6);snap('popup-front-opengl')
for _ in range(3):icon('下一个标注')
time.sleep(.6);behind=snap('behind-camera-hidden');assert not contains(behind,'annotation-popup')
icon('下一个标注');time.sleep(.6);off=snap('offscreen-popup-clamped');assert contains(off,'annotation-popup')
popup=next(n for n in nodes(off) if n.get('id')=='annotation-popup');x,y,X,Y=popup['bounds'];assert 0<=x<X<=1320 and 0<=y<Y<=2848
icon('关闭标注');icon('设置');click('性能信息');depth=snap('depth-capability');assert any('标注深度：24 bit' in n.get('text','') or '标注深度：16 bit' in n.get('text','') for n in nodes(depth))
if a.include_huawei:
 icon('模型');click('切换华为原生');time.sleep(2);icon('模型');snap('fixture-huawei-fallback')
 icon('annotation-1');time.sleep(.6);snap('popup-huawei')
 shell('uitest uiInput keyEvent Home');time.sleep(1);shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(3);snap('huawei-resumed')
 icon('模型');click('切换 OpenGL');time.sleep(2);icon('模型');snap('opengl-recreated')
else:
 icon('设置')
 shell('uitest uiInput keyEvent Home');time.sleep(1);shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(3);snap('opengl-resumed')
if contains(layout(),'annotation-popup'):icon('关闭标注')
icon('隐藏工具');hidden=snap('hotspots-hidden');assert not contains(hidden,'annotation-0')
icon('显示工具');snap('hotspots-restored')
(a.out/'hilog.txt').write_text(shell('hilog -x -P '+pid));(a.out/'memory.txt').write_text(shell('hidumper --mem '+pid))
(a.out/'result.json').write_text(json.dumps({'fixture':'3 Gaussian occluders, 5 world-space annotations','functionalUi':'passed','openglDepth':'available','huaweiDepth':'unverified; 2D fallback' if a.include_huawei else 'excluded by user scope','visualReviewRequired':True,'performanceAcceptance':False},indent=2))
print('Annotation functional checks complete. Inspect screenshots for Gaussian occlusion and glyph quality.')
