const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const base='apps/harmonyos/entry/src/main/ets/pages/';
function read(name,deps={}) {const box={exports:{},require:n=>{if(n in deps)return deps[n];throw Error(n)}};vm.runInNewContext(ts.transpileModule(fs.readFileSync(base+name+'.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);return box.exports;}
const spline=read('ViewerSpline'),settings=read('ViewerSettings',{'./ViewerSpline':spline});
const profile=read('ViewerProfile',{'@kit.AbilityKit':{},'@kit.ArkTS':{},'./ViewerSettings':settings});
const runtime=read('ViewerRuntime',{'./OrbitControls':read('OrbitControls'),'./ViewerProfile':profile,'./ViewerSettings':settings});
const copy=x=>JSON.parse(JSON.stringify(x)); const close=(a,b,e=1e-8)=>assert.ok(Math.abs(a-b)<e,`${a} != ${b}`);
let raw=copy(settings.createSettings()); raw.annotations=[{position:[10,20,30],title:'A',text:'<b>literal</b>',camera:{initial:{position:[10,20,40],target:[10,20,30],fov:75}},extras:{asset:'unchanged'}}];
let parsed=settings.parseViewerSettings(JSON.stringify(raw));assert.equal(parsed.cameras.length,0);assert.equal(parsed.annotations[0].extras.asset,'unchanged');
const p=new profile.ViewerProfile(parsed,[10,20,30,2]);assert.deepEqual(copy(p.initial().target),[10,20,30]);assert.equal(p.initial().fov,85);
let legacy=settings.parseViewerSettings(JSON.stringify({camera:{fov:70,startAnim:'animTrack'},animTracks:[{name:'legacy',duration:2,loopMode:'none',interpolation:'step',keyframes:{times:[0,1],values:{position:[0,0,2,1,0,2],target:[0,0,0,1,0,0]}}}]}));
assert.equal(legacy.animTracks[0].frameRate,30);assert.equal(legacy.animTracks[0].keyframes.times[1],30);assert.equal(legacy.tonemapping,'none');
let anim=new settings.ViewerAnimation(legacy.animTracks[0]);close(anim.sample(.999).position[0],0);close(anim.sample(1).position[0],1);close(anim.sample(10).position[0],1);
let single=copy(legacy.animTracks[0]);single.keyframes.times=[0];single.keyframes.values={position:[0,0,2],target:[0,0,0],fov:[70]};single.interpolation='spline';assert.equal(new settings.ViewerAnimation(single).sample(1).fov,70);
for(const change of [s=>s.annotations[0].position[0]=null,s=>s.annotations[0].camera.initial.fov=180,s=>s.annotations[0].text=4,s=>s.postEffectSettings.grading.tint=[1,2],s=>s.tonemapping='garbage',s=>s.soundUrl=3]) {const bad=copy(raw);change(bad);assert.throws(()=>settings.parseViewerSettings(JSON.stringify(bad)));}
const r=new runtime.ViewerRuntime();r.bounds=[10,20,30,2];r.controls.resize(400,600);r.configure(parsed);const pose=parsed.annotations[0].camera.initial;
for(const fly of [true,false]) {r.apply(pose,fly);const out=r.pose();for(let i=0;i<3;i++){close(out.position[i],pose.position[i]);close(out.target[i],pose.target[i]);}const marker=r.markers()[0];close(marker.x,200);close(marker.y,300);}
r.apply({position:[10,20,20],target:[10,20,10],fov:75},true);assert.equal(r.markers().length,0);
r.apply(pose,true);r.gaming=true;r.navigate([1,0,0],true);assert.equal(r.controls.fly,true);r.gaming=false;r.navigate([1,0,0],true);for(let i=0;i<40;i++)r.tick(16);assert.equal(r.controls.fly,false);close(r.pose().target[0],12);
r.annotation(0);for(let i=0;i<40;i++)r.tick(16);close(r.pose().position[0],10);assert.equal(r.selected,0);
r.apply(pose,true);r.navigate([1,0,0],false);for(let i=0;i<40;i++)r.tick(16);assert.equal(r.controls.fly,true);assert.ok(r.pose().position[0]>10);assert.ok(r.pose().position[0]<12);
const real=settings.parseViewerSettings(fs.readFileSync('apps/harmonyos/entry/src/main/resources/rawfile/viewer-settings.json','utf8'));assert.equal(real.annotations.length,10);
console.log('PASS: legacy migration, empty-camera fit, metadata retention, step boundaries, one-key track, malformed settings, world-space round trips, projection/behind-camera clipping, annotation transition, official click navigation and gaming suppression');

r.apply({position:[10,20,40],target:[10,20,30],fov:75},true);r.transition({position:[10,20,20],target:[10,20,30],fov:75});for(let i=0;i<40;i++){r.tick(16);const q=r.pose();assert.ok(Math.hypot(...q.position.map((v,k)=>v-q.target[k]))>9.9);assert.ok(q.position.every(Number.isFinite));}close(r.pose().position[2],20);

for (const radius of [.001, 1, 1000]) {
 const z=new runtime.ViewerRuntime(); z.bounds=[0,0,0,radius];
 z.controls.zoom(1e-10); z.controls.tick(64);
 close(z.controls.target[2]*3*radius,.01);
 z.controls.zoom(1e9); assert.ok(z.controls.target[2]>20);
 const before=z.controls.target[2]; for(const bad of [NaN,Infinity,0,-1])z.controls.zoom(bad);close(z.controls.target[2],before);
}
console.log('PASS world-unit orbit minimum across scene scales, unlimited outward zoom, invalid zoom rejection');

// Joystick/button movement is world-space, rate based, with no release tail.
for(const radius of [.1,1,1000])for(const frames of [30,60,120]) {
 const r=new runtime.ViewerRuntime();r.bounds=[0,0,0,radius];r.controls.setFly(true);
 const start=r.pose().position.slice();r.controls.axes=[0,0,1];
 for(let i=0;i<frames;i++)r.tick(1000/frames);
 close(r.pose().position[2]-start[2],-4,1e-8);
 r.controls.axes=[0,0,0];const stopped=r.pose().position.slice();for(let i=0;i<60;i++)r.tick(1000/60);
 stopped.forEach((v,i)=>close(r.pose().position[i],v));
 r.controls.axes=[0,1,0];r.tick(50);close(r.pose().position[1]-stopped[1],.2);
 r.controls.cancel();r.tick(50);close(r.pose().position[1]-stopped[1],.2);
}
const fly=new runtime.ViewerRuntime();fly.controls.setFly(true);fly.controls.key('w',true);fly.tick(16);
const first=fly.controls.current[5];assert.ok(first<3&&first>2.99,'keyboard eases into movement');
fly.controls.key('w',false);for(let i=0;i<100;i++)fly.tick(16);const stopped=fly.controls.current[5];fly.tick(16);close(fly.controls.current[5],stopped);
console.log('PASS 4 world units/sec across scales and frame rates, release/cancel stop, held rise, keyboard acceleration/deceleration');

for(const radius of [.1,1000]) {
 const r=new runtime.ViewerRuntime();r.bounds=[0,0,0,radius];r.controls.resize(400,800);r.controls.setFly(true);
 const start=r.pose().position.slice();r.controls.wheel(-120);r.tick(16);close(r.pose().position[2]-start[2],-.12);
 const stopped=r.pose().position.slice();r.tick(16);close(r.pose().position[2],stopped[2]);
 r.controls.zoom(Math.exp(-.2));r.tick(16);close(r.pose().position[2]-stopped[2],-.2);
 const before=r.pose().position.slice();
 r.controls.touch([{id:1,x:100,y:100},{id:2,x:200,y:100}],false);
 r.controls.touch([{id:1,x:70,y:100},{id:2,x:230,y:100}],false);r.tick(16);close(r.pose().position[2]-before[2],-.4);
 const gaming=r.pose().position.slice();
 r.controls.touch([{id:1,x:50,y:200},{id:2,x:250,y:200}],false,true);r.tick(16);
 gaming.forEach((v,i)=>close(r.pose().position[i],v));
 r.controls.wheel(-120);r.controls.cancel();r.tick(16);gaming.forEach((v,i)=>close(r.pose().position[i],v));
}
console.log('PASS world-space wheel/button/pinch displacements, one-shot consumption, gaming two-finger isolation and cancellation');

for (const inside of [false,true]) for (const explicit of [false,true]) for (const preferred of ['', 'orbit','fly','walk','anim']) for (const forced of ['', 'figure8']) {
 const cfg=settings.createSettings({position:[0,0,inside?.5:5],target:[0,0,0],fov:75});
 if(explicit){cfg.startMode='animTrack';cfg.animTracks=[settings.defaultTrack(cfg.cameras[0].initial,false)];}
 const p=new profile.ViewerProfile(cfg,[0,0,0,1],{mode:preferred,syntheticAnimation:forced,noanim:false});
 const animated=explicit||forced==='figure8'||inside;
 const expected=preferred==='anim'||animated?'anim':preferred==='fly'||preferred==='walk'?'fly':'orbit';
 assert.equal(p.initialMode,expected);
 if(!explicit)assert.equal(p.animation.sample(0).fov,75);
 const paused=new profile.ViewerProfile(cfg,[0,0,0,1],{mode:preferred,syntheticAnimation:forced,noanim:true});
 assert.notEqual(paused.initialMode,'anim');
}
console.log('PASS MetaFlow explicit/synthetic animation startup, preferred navigation modes and noanim gate');
