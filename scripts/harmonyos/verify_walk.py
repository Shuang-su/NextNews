#!/usr/bin/env python3
"""Real-phone walk preview on the actual Huafa 32-tile data. Source env.sh.
Requires the scene server on 8768 and actual voxel server on 8780 (HDC rport).
Saves raw evidence; it does not declare full walk/collision acceptance.
"""
import argparse,json,subprocess,time,re
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--collision-url');p.add_argument('--debug',action='store_true');p.add_argument('--backend',choices=['OpenGL','Huawei'],default='OpenGL');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
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
def icon(label):ui('click','--id',label);time.sleep(.4)
def click(label):
 for i in range(6):
  n=next((n for n in nodes(layout()) if n.get('text')==label),None)
  if n:
   x,y,X,Y=n['bounds'];ui('click',str((x+X)//2),str((y+Y)//2));time.sleep(.6);return
  ui('swipe','1000','2200','1000','1520','--speed','1000')
 raise RuntimeError('Missing control '+label)
def snap(name):
 data=layout();(a.out/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2));ui('screenshot','--path',str(a.out/(name+'.png')));return data
run([hdc,'-t',device,'rport','tcp:8781','tcp:8781']);run([hdc,'-t',device,'rport','tcp:8768','tcp:8768']);run([hdc,'-t',device,'rport','tcp:8780','tcp:8780'])
shell('aa force-stop com.nextnews.splatviewer');shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(2)
pid=shell('pidof com.nextnews.splatviewer').strip().split()[0]
record=(a.out/'continuous-hilog.txt').open('wb');logger=subprocess.Popen([hdc,'-t',device,'shell','hilog -P '+pid],stdout=record,stderr=subprocess.STDOUT)
try:
 (a.out/'device.txt').write_text(shell('param get const.product.model')+shell('param get const.ohos.apiversion')+shell('param get const.product.software.version'))
 if a.backend == 'OpenGL':
  icon('设置');click('流式：兼容');click('纹理：浮点');icon('设置');icon('模型');click('流式');click('连接流式');icon('模型');time.sleep(5)
 else:
  icon('模型');click('转换样例');time.sleep(1);click('配置默认镜头');click('切换华为原生');time.sleep(2);icon('模型')
 before=snap('scene-before-walk')
 assert not any('流式失败' in n.get('text','') for n in nodes(before)), 'Scene load failed; do not test collision against a previous model'
 assert any(n.get('id')=='下一个标注' for n in nodes(before)), 'Scene profile was not loaded'
 icon('模型');click('碰撞 / 体素（实验）');
 if a.collision_url:
  ui('click','--id','collision-address');shell('uitest uiInput keyEvent 2072 2017');ui('text',a.collision_url);shell('uitest uiInput keyEvent Back');time.sleep(.5)
 click('加载碰撞');time.sleep(8);collision=snap('collision-loaded');
 assert any('碰撞已缓存' in n.get('text','') for n in nodes(collision)), 'Collision did not load; do not proceed to walk'
 if a.debug:click('显示碰撞边线');time.sleep(1)
 icon('模型');icon('步行');time.sleep(3);snap('walk-spawn')
 icon('设置');click('游戏控制：关');icon('设置');state=snap('walk-gaming')
 assert not any(n.get('id')=='动画时间轴' for n in nodes(state)), 'Animation timeline must not control the walking body'
 stick=next(n for n in nodes(state) if n.get('id')=='飞行摇杆');x,y,X,Y=stick['bounds'];sx=(x+X)//2;sy=(y+Y)//2
 for _ in range(4):ui('swipe',str(sx),str(sy),str(sx+15),str(sy-100),'--speed','200');time.sleep(.2)
 snap('walk-moved')
 if a.debug:
  ui('swipe','900','1300','900','1700','--speed','1000');time.sleep(1);snap('collision-look-down')
  ui('swipe','900','1700','900','1300','--speed','1000');time.sleep(.5)
 click('跳跃');time.sleep(.15);snap('walk-jump');time.sleep(1)
 shell('uitest uiInput keyEvent Home');time.sleep(1);shell('aa start -b com.nextnews.splatviewer -a EntryAbility');time.sleep(5);snap('walk-resumed');icon('飞行');snap('walk-exit')
 if a.debug:
  icon('播放动画');time.sleep(.5);animated=snap('animation-playing')
  slider=next(n for n in nodes(animated) if n.get('id')=='动画时间轴');x,y,X,Y=slider['bounds'];sy=(y+Y)//2
  ui('swipe',str(x+20),str(sy),str(X-30),str(sy),'--speed','300');time.sleep(.3)
  resumed=snap('timeline-resumes-playing');assert any(n.get('id')=='暂停动画' for n in nodes(resumed)), 'Scrubbing must resume an originally playing animation'
  icon('暂停动画');ui('swipe',str(X-30),str(sy),str(x+30),str(sy),'--speed','300');time.sleep(.3)
  paused=snap('timeline-stays-paused');assert any(n.get('id')=='播放动画' for n in nodes(paused)), 'Paused animation must remain paused after scrubbing'
  icon('飞行');final=snap('timeline-hidden-fly');assert not any(n.get('id')=='动画时间轴' for n in nodes(final))
  icon('模型');click('关闭碰撞显示');icon('模型');off=snap('debug-off')
  assert not any(n.get('text','').startswith('碰撞透视') for n in nodes(off)), 'Debug HUD should disappear; inspect screenshot for cleared wire lines'
 (a.out/'hilog.txt').write_text(shell('hilog -x -P '+pid));(a.out/'memory.txt').write_text(shell('hidumper --mem '+pid))
 print('Walk preview captures finished. Review geometry, actual movement and logs; full acceptance remains separate.')
finally:
 logger.terminate()
 try:logger.wait(timeout=5)
 except subprocess.TimeoutExpired:logger.kill();logger.wait()
 record.close()
log=(a.out/'continuous-hilog.txt').read_text(errors='replace')
poses=[{k:float(v) for k,v in re.findall(r'(x|y|z|grounded|blocked)=([-\d.]+)',l)} for l in log.splitlines() if 'ViewerWalk ' in l]
result={'backend':a.backend,'collisionSource':a.collision_url or 'actual 32-tile voxel manifest','scope':'whole-scene 2M' if a.backend=='OpenGL' else '80k single converted sample; not whole-scene comparison','samples':len(poses),'performanceAcceptance':False,'visualReviewRequired':True}
if a.debug:result['timelineSmoke']='resume-playing, remain-paused, hidden-in-walk/fly';result['debugOffVisualReviewRequired']=True
if poses:
 result['first']=poses[0];result['last']=poses[-1];result['ranges']={k:[min(p[k] for p in poses),max(p[k] for p in poses)] for k in ['x','y','z']}
(a.out/'result.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result))
