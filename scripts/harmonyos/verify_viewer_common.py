#!/usr/bin/env python3
"""Device-only smoke evidence for the common viewer. Source env.sh first.
Does not treat compiler success or UI text as evidence of actual model rendering.
Review the captured images separately. Output directory must be new.
"""
import argparse,json,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--include-huawei',action='store_true',help='Historical opt-in only; Huawei research was discontinued');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
hdc='/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc'
def run(args):return subprocess.run(args,check=True,capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=60).stdout
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
 for i in range(4):
  n=next((n for n in nodes(layout()) if n.get('text')==label and n['bounds'][2]>n['bounds'][0] and n['bounds'][3]>n['bounds'][1]),None)
  if n:
   x,y,X,Y=n['bounds'];ui('click',str((x+X)//2),str((y+Y)//2));time.sleep(.5);return
  ui('swipe','1000','2210','1000','1550','--speed','1000')
 raise RuntimeError('Missing control '+label)
def snap(name):
 data=layout();(a.out/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2));ui('screenshot','--path',str(a.out/(name+'.png')));return data
shell('aa force-stop com.nextnews.splatviewer');shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(2)
pid=shell('pidof com.nextnews.splatviewer').strip().split()[0]
(a.out/'device.txt').write_text(shell('param get const.product.model')+shell('param get const.ohos.apiversion')+shell('param get const.product.software.version'))
icon('模型');click('公开样例');time.sleep(3);snap('public-opengl')
if a.include_huawei:
 icon('模型');click('切换华为原生');time.sleep(2);icon('模型');snap('public-huawei')
 icon('模型');click('切换 OpenGL');time.sleep(2)
else:icon('模型')
click('转换样例');time.sleep(3);icon('模型');click('配置默认镜头');icon('模型');before=snap('annotations-overview')
assert any(n.get('id')=='下一个标注' for n in nodes(before))
icon('下一个标注');time.sleep(.7);active=snap('annotation-opengl')
assert any(n.get('text','').startswith('1 / 10') for n in nodes(active))
icon('播放动画');time.sleep(1);assert any(n.get('id')=='暂停动画' for n in nodes(layout()));icon('暂停动画')
slider=next(n for n in nodes(layout()) if n.get('id')=='动画时间轴');x,y,X,Y=slider['bounds'];ui('click',str(round(x+(X-x)*.7)),str((y+Y)//2));snap('timeline-scrub')
if a.include_huawei:
 icon('模型');click('切换华为原生');time.sleep(2);icon('模型');snap('annotation-huawei')
shell('uitest uiInput keyEvent Home');time.sleep(1);shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(2);snap('huawei-resumed' if a.include_huawei else 'opengl-resumed')
if a.include_huawei:
 icon('模型');click('切换 OpenGL');time.sleep(2);icon('模型')
icon('飞行');icon('设置');click('游戏控制：关');icon('设置');snap('gaming-controls')
(a.out/'hilog.txt').write_text('\n'.join(l for l in shell('hilog -x').splitlines() if len(l.split())>2 and l.split()[2]==pid))
(a.out/'memory.txt').write_text(shell('hidumper --mem '+pid))
(a.out/'result.json').write_text(json.dumps({'uiSmoke':'passed','annotations':10,'backends':['OpenGL','Huawei'] if a.include_huawei else ['OpenGL'],'visualReviewRequired':True,'performanceGatesTested':False},indent=2))
print('UI and lifecycle smoke completed; inspect screenshots before claiming visual parity.')
