const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const cache={};
function read(name){if(cache[name])return cache[name];const c={exports:{},require:n=>n.startsWith('./')?read(n.slice(2)): {}};vm.runInNewContext(ts.transpileModule(fs.readFileSync(`apps/harmonyos/entry/src/main/ets/pages/${name}.ets`,'utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,c);return cache[name]=c.exports;}
const {annotationPopup}=read('ViewerAnnotations');
assert.equal(annotationPopup(100,300,600,800,100).x,125);
assert.equal(annotationPopup(500,300,600,800,100).flipped,true);
assert.equal(annotationPopup(500,300,600,800,100).x,275);
for(const [w,h] of [[400,800],[800,400],[180,280]])for(const x of [-1e5,0,w/2,w,1e5])for(const y of [-1e5,0,h/2,h,1e5]){
 const p=annotationPopup(x,y,w,h,100);
 assert.ok(p.x>=8&&p.x+p.width<=w-8);assert.ok(p.y>=8&&p.y+100<=h-8);assert.ok(p.arrow>=16&&p.arrow<=84);
}
const {ViewerRuntime}=read('ViewerRuntime'),{parseViewerSettings}=read('ViewerSettings');
const r=new ViewerRuntime();r.controls.resize(400,800);r.configure(parseViewerSettings(fs.readFileSync('apps/harmonyos/entry/src/main/resources/rawfile/annotation-fixture.json','utf8')));r.initial();
assert.deepEqual(JSON.parse(JSON.stringify(r.markers().map(m=>m.index))),[0,1,2]);
assert.equal(r.anchor(3),undefined);assert.equal(r.anchor(-1),undefined);assert.equal(r.anchor(5),undefined);
assert.ok(r.anchor(4).x>400);assert.ok(r.anchor(1).depth>r.anchor(0).depth);
const near=r.profile.settings.annotations[3];near.position=[0,0,5.99999];assert.ok(r.anchor(3));
r.annotation(1);for(let i=0;i<40;i++)r.tick(16);assert.equal(r.selected,1);assert.equal(r.anchor(1).text,'编号 2。<b>保持纯文本</b>。遮挡后仍可点击。');
console.log('PASS annotation projection, near clamp semantics, behind-camera hiding, offscreen active anchor, popup flip/clamp and plain text');
