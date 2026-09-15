const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
function read(name){const c={exports:{},require:()=>({})};vm.runInNewContext(ts.transpileModule(fs.readFileSync(`apps/harmonyos/entry/src/main/ets/pages/${name}.ets`,'utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,c);return c.exports;}
const {ViewerProjection}=read('ViewerProjection'),{ViewerScrub}=read('ViewerScrub');
const project=new ViewerProjection({position:[0,0,0],target:[0,0,-1],fov:90},400,600);
assert.equal(project.segment([0,0,1],[1,0,2]).length,0);
for(const [a,b] of [[[0,0,-1],[1,0,-1]],[[-1,0,1],[.1,0,-1]],[[-100,100,-1],[100,-100,-1]],[[0,0,-1],[0,0,-2]]]){
 const line=project.segment(a,b);assert.equal(line.length,4);line.forEach((v,i)=>assert.ok(Number.isFinite(v)&&v>=-1e-6&&v<=(i%2?600:400)+1e-6));
}
const scrub=new ViewerScrub();
assert.equal(scrub.begin([{id:9,x:350,y:-10}],300),undefined);
assert.equal(scrub.begin([{id:5,x:60,y:20}],300),.2);
assert.equal(scrub.begin([{id:2,x:240,y:20}],300),undefined);
assert.equal(scrub.move([{id:2,x:240,y:20},{id:5,x:90,y:20}],300),.3);
assert.equal(scrub.release([{id:2,x:240,y:20}]),false);assert.equal(scrub.owner,5);
assert.equal(scrub.move([{id:5,x:400,y:20}],300),1);
assert.equal(scrub.release([{id:5,x:400,y:20}]),true);assert.equal(scrub.owner,-1);
scrub.begin([{id:3,x:5,y:20}],300);scrub.cancel();assert.equal(scrub.move([{id:3,x:200,y:20}],300),undefined);
console.log('PASS near-plane/viewport clipping and independent timeline pointer ownership, reordered touches, unrelated release, cancel');
