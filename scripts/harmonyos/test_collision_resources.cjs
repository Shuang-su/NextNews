const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict'),ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const load=(name,deps={})=>{const box={exports:{},require:n=>deps[n]||{}};vm.runInNewContext(ts.transpileModule(fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/'+name+'.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);return box.exports;};
const {viewerRequests:pool}=load('ViewerRequests');
const {validateCollisionManifest:validate}=load('ViewerCollision',{'./ViewerRequests':{viewerRequests:pool}});
const m={version:1,voxelResolution:.08,tiles:[{id:'tile',ix:0,iz:0,coreBounds:{min:[0,0,0],max:[64,32,64]},url:'tiles/tile/surface.voxel.json'}]};validate(m);
for(const change of [x=>x.tiles.push({...x.tiles[0]}),x=>x.tiles[0].coreBounds.max[0]=0,x=>x.tiles[0].ix=Infinity,x=>x.voxelResolution=0,x=>x.tiles[0].url='test.bin',x=>x.tiles=null]){const bad=JSON.parse(JSON.stringify(m));change(bad);assert.throws(()=>validate(bad));}
(async()=>{
 let active=0,peak=0,done=0;
 const jobs=Array.from({length:16},(_,i)=>(async()=>{await pool.acquire();active++;peak=Math.max(peak,active);try{await new Promise(r=>setTimeout(r,1));if(i%3===0)throw Error('simulated cancellation');}catch{}finally{active--;done++;pool.release();}})());
 await Promise.all(jobs);assert.equal(done,16);assert.equal(peak,4);assert.equal(active,0);
 await pool.acquire();pool.release();console.log('PASS collision manifests, malformed entries and shared splat/collision four-request limit including cancellation');
})().catch(e=>{console.error(e);process.exitCode=1;});
