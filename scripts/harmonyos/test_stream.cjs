const fs=require('fs'), assert=require('node:assert/strict'),vm=require('vm');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const source=fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/StreamSession.ets','utf8');
const box={exports:{},require:()=>({})};
vm.runInNewContext(ts.transpileModule(source,{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
const validate=box.exports.validateManifest;
const base={version:1,bounds:[0,0,0,1],chunks:[{file:'chunk-0000.ply',count:100,bytes:6000,bounds:[0,0,0,1]}]};
validate(base);
for(const mutate of [m=>m.bounds[0]=NaN,m=>m.bounds[3]=0,m=>m.chunks[0].file='../secret',m=>m.chunks[0].count=4000001,m=>m.chunks[0].bytes=33554433,m=>m.chunks.push(m.chunks[0]),m=>m.chunks=null]){
 const m=JSON.parse(JSON.stringify(base));mutate(m);assert.throws(()=>validate(m));
}
console.log('PASS stream manifest: bounds, traversal, chunk count, byte budget, duplicate paths, invalid arrays');

const m={version:2,bounds:[0,0,0,10],levels:3,chunks:[
 {file:'chunk-0000.sog',count:1000,bytes:10000,bounds:[0,0,0,10]},
 {file:'chunk-0001.sog',count:2000,bytes:20000,bounds:[0,0,0,10]}],leaves:[
 {bounds:[0,0,0,1],lods:[[1,0,1000],[0,0,500],[0,500,100]]},
 {bounds:[5,0,0,1],lods:[[1,1000,1000],[0,600,400],[-1,0,0]]}]};
validate(m);
for(const budget of [100,500,1000,2000]) {
 const levels=box.exports.selectLods(m,[0,0,1,0,0,0],false,budget);
 assert.equal(levels.length,m.leaves.length);let count=0;
 for(let i=0;i<levels.length;i++){const r=m.leaves[i].lods[levels[i]];count+=r[2];}
 assert.ok(count<=budget);assert.ok(count>=100);
}
for(const change of [m=>m.leaves[0].lods[0][1]=2001,m=>m.leaves[0].lods[0][0]=9,m=>m.leaves[0].lods[0][2]=-1]) {
 const bad=JSON.parse(JSON.stringify(m));change(bad);assert.throws(()=>validate(bad));
}
console.log('PASS LOD: bounded selection, sparse coarse levels, range and file validation');
if(process.argv[2]) {
 const full=JSON.parse(fs.readFileSync(process.argv[2]));validate(full);
 const chosen=box.exports.selectLods(full,[0,0,1,0,0,0],false,2000000);
 const count=chosen.reduce((n,l,i)=>n+full.leaves[i].lods[l][2],0);assert.ok(count<=2000000);
 console.log('PASS source hierarchy',full.leaves.length,count);
}
// Equal-size leaves: rear penalty prioritizes the front while retaining offscreen detail.
const view={version:2,bounds:[0,0,0,10],levels:2,
 chunks:[{file:'chunk-0000.sog',count:3000,bytes:30000,bounds:[0,0,0,10]}],
 leaves:[{bounds:[0,0,-5,.2],lods:[[0,0,900],[0,900,10]]},
 {bounds:[0,0,5,.2],lods:[[0,1000,900],[0,1900,10]]},
 {bounds:[3,0,-5,.2],lods:[[0,2000,900],[0,2900,10]]}]};
validate(view);
assert.deepEqual(Array.from(box.exports.selectLods(view,[0,0,1,0,0,0],true,2000,45,1)),[0,1,0]);
assert.deepEqual(Array.from(box.exports.selectLods(view,[0,0,1,0,0,0],true,2000,75,1)),[0,1,0]);
assert.deepEqual(Array.from(box.exports.selectLods(view,[Math.PI,0,1,0,0,0],true,1000,45,1)),[1,0,1]);
const malformed=JSON.parse(JSON.stringify(view));malformed.leaves[0].aabb=[1,0,0,-1,1,1];assert.throws(()=>validate(malformed));
const nearby=JSON.parse(JSON.stringify(view));
nearby.leaves[2].aabb=[-.01,-.01,-.01,100,.01,.01]; // camera intersects a long thin box
assert.equal(box.exports.selectLods(nearby,[0,0,1,0,0,0],true,1000,45,1)[2],0);
console.log('PASS AABB distance, rear penalty, offscreen detail, fixed budget, invalid box');
(async()=>{
 let inflight=0,peak=0,published=0;
 const fakeFs={OpenMode:{CREATE:1,READ_WRITE:2,TRUNC:4},open:async()=>({fd:1}),write:async(_fd,b)=>b.byteLength,close:async()=>{},rename:async()=>{},unlink:async()=>{}};
 const render={status:()=>({state:'ready'}),chunks:()=>published++};
 const sandbox={exports:{},require:n=>n==='@kit.CoreFileKit'?{fileIo:fakeFs}:n==='libsplat.so'?{default:render}:{}};
 vm.runInNewContext(ts.transpileModule(source,{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,sandbox);
 const session=new sandbox.exports.StreamSession(()=>{});
 session.manifest={version:2,bounds:[0,0,0,1],levels:1,chunks:Array.from({length:6},(_,i)=>({file:`chunk-000${i}.sog`,count:10,bytes:16,bounds:[0,0,0,1]})),leaves:[{bounds:[0,0,0,1],lods:[[0,0,10]]}]};
 session.fullLoad=true;
 session.fetch=async()=>{inflight++;peak=Math.max(peak,inflight);await new Promise(setImmediate);inflight--;return new ArrayBuffer(16);};
 await session.pump([0,0,1,0,0,0],false,45,1);
 assert.equal(peak,4);assert.equal(session.cache.size,4);assert.equal(published,1,'full preload must publish before all files arrive');
 await session.pump([0,0,1,0,0,0],false,45,1);
 assert.equal(session.cache.size,6);assert.equal(published,1,'unchanged scene should not reupload');
 console.log('PASS streaming scheduler: four requests, early full-preload frame, unchanged selection');
 const turning=new sandbox.exports.StreamSession(()=>{});
 turning.manifest=view;turning.budget=1000;turning.busy=true;
 turning.cache.set('chunk-0000.sog','/cache/chunk-0000.sog');
 const beforeTurn=published;
 turning.tick([0,0,1,0,0,0],true,45,1);
 turning.tick([Math.PI,0,1,0,0,0],true,45,1);
 assert.equal(published,beforeTurn+2,'cached front/back selections must publish during pending downloads');
 console.log('PASS cached camera turn does not wait for the network batch');

 const pressure=new sandbox.exports.StreamSession(()=>{}),removed=[];
 fakeFs.unlink=async path=>removed.push(path);
 pressure.manifest={version:2,bounds:[0,0,0,1],levels:3,
  chunks:Array.from({length:4},(_,i)=>({file:`chunk-000${i}.sog`,count:10,bytes:16,bounds:[0,0,0,1]})),
  leaves:[{bounds:[0,0,0,1],lods:[[2,0,10],[1,0,10],[0,0,10]]}]};
 pressure.updateSelection=()=>{};pressure.selection=[1];pressure.publishCached=()=>[];
 pressure.targetFiles=new Set(['chunk-0001.sog']);pressure.previousFiles=new Set(['chunk-0002.sog']);
 for(let i=0;i<4;i++)pressure.cache.set(`chunk-000${i}.sog`,`/cache/chunk-000${i}.sog`);
 pressure.loadedBytes=536870912+16;
 await pressure.pump([0,0,1,0,0,0],true,75,1);
 assert.deepEqual(removed,['/cache/chunk-0003.sog']);
 assert.ok(pressure.cache.has('chunk-0002.sog'),'previous view stays pinned');
 pressure.cache.delete('chunk-0002.sog');let requests=0;pressure.fetch=async()=>{requests++;return new ArrayBuffer(16);};
 await pressure.pump([0,0,1,0,0,0],true,75,1);
 assert.equal(requests,0,'full cache must not fetch and immediately evict prefetch');
 console.log('PASS disk pressure: current/previous views pinned; no prefetch churn without headroom');

})().catch(e=>{console.error(e);process.exitCode=1;});
