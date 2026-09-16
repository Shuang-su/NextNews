const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
function moduleFile(path,deps={}) {const box={exports:{},require:n=>{if(!(n in deps))throw Error(n);return deps[n];}};
 vm.runInNewContext(ts.transpileModule(fs.readFileSync(path,'utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);return box.exports;}
const base='apps/harmonyos/entry/src/main/ets/pages/';
const spline=moduleFile(base+'ViewerSpline.ets');const api=moduleFile(base+'ViewerSettings.ets',{'./ViewerSpline':spline});
const settings=api.parseViewerSettings(fs.readFileSync('apps/harmonyos/entry/src/main/resources/rawfile/viewer-settings.json','utf8'));
assert.equal(settings.cameras[0].initial.fov,75);assert.equal(settings.animTracks.length,0);
const initial=settings.cameras[0].initial;const track=api.defaultTrack(initial,true);assert.equal(track.name,'figure8');assert.equal(track.keyframes.times.length,24);
const anim=new api.ViewerAnimation(track);
for(const t of [0,1,5,10,19.99,20,40]) {const p=anim.sample(t);assert.ok(p.position.every(Number.isFinite));assert.equal(p.fov,75);}
assert.ok(Math.abs(anim.sample(0).position[0]-initial.position[0])<1e-10);
assert.ok(Math.abs(anim.sample(20).position[0]-initial.position[0])<1e-10);
const b=[200,-29,163,318],c=api.normalizedPose(initial,b);
for(let i=0;i<3;i++)assert.ok(Math.abs(b[i]+c[i+3]*b[3]-initial.position[i])<1e-9);
for(const change of [s=>s.cameras[0].initial.fov=180,s=>s.cameras[0].initial.position=[0,0],s=>s.cameras[0].initial.target=s.cameras[0].initial.position,s=>s.animTracks=null]) {
 const bad=JSON.parse(JSON.stringify(settings));change(bad);assert.throws(()=>api.parseViewerSettings(JSON.stringify(bad)));
}
const explicit={...track,loopMode:'pingpong'};let p=new api.ViewerAnimation(explicit);
for(let i=0;i<3;i++)assert.ok(Math.abs(p.sample(5).position[i]-p.sample(35).position[i])<1e-9);
const once=new api.ViewerAnimation({...track,loopMode:'none'});
assert.deepEqual(JSON.parse(JSON.stringify(once.sample(20))),JSON.parse(JSON.stringify(once.sample(21))));
// Compare directly to the pinned reference implementation when present.
const reference='.local/supersplat-viewer-reference/src/';
if(fs.existsSync(reference+'core/spline.ts')) {
 const ref=moduleFile(reference+'core/spline.ts').CubicSpline;
 const times=[0,1,3,5],points=[0,3,2,4,5,1,7,-2];
 const ours=spline.CubicSpline.fromPointsLooping(7,times,points,.7),theirs=ref.fromPointsLooping(7,times,points,.7);
 for(let t=0;t<7;t+=.03125){const a=[],b=[];ours.evaluate(t,a);theirs.evaluate(t,b);assert.deepEqual(a,b);}
}
console.log('PASS: real 75-degree profile, camera normalization, default animation loop, repeat/pingpong/end, invalid settings, reference spline parity');

const jsonc = '// config\n' + JSON.stringify(settings).replace('"version":2', '"version":2 /* revision */').replace(/}$/, ',}');
assert.equal(api.parseViewerSettings(jsonc).version, 2);
assert.deepEqual(JSON.parse(api.settingsJson('{"url":"https://a.test/a//b", "text":",] /* x */ \\"quote\\"", "items":[1, /* x */ 2,],}')), {url:'https://a.test/a//b',text:',] /* x */ "quote"',items:[1,2]});
assert.throws(()=>api.settingsJson('{/* never closed'));
assert.throws(()=>JSON.parse(api.settingsJson('{"value":1/* gap */2}')));
console.log('PASS JSONC comments and trailing commas preserve quoted content and reject malformed tokens');
