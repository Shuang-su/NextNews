const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const root='apps/harmonyos/entry/src/main/ets/pages/',cache={};let current;
const network={http:{createHttp:()=>current,HttpDataType:{ARRAY_BUFFER:0}}};
function load(name){if(cache[name])return cache[name];const box={exports:{},require:n=>n.startsWith('./')?load(n.slice(2)):n==='@kit.ArkTS'?{url:{URL}}:n==='@kit.NetworkKit'?network:{},ArrayBuffer,Uint8Array,Map,Set,Error,Promise};cache[name]=box.exports;vm.runInNewContext(ts.transpileModule(fs.readFileSync(root+name+'.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);return box.exports;}
const {describeScene}=load('ViewerScene'),{createSettings}=load('ViewerSettings'),{ViewerCatalog}=load('ViewerCatalog');
const settings=createSettings();settings.voxelManifestUrl='../voxels/index.json';settings.soundUrl='audio/theme.mp3';settings.background.skyboxUrl='/sky.hdr';
const before=JSON.stringify(settings),scene=describeScene('https://cdn.test/model.sog','https://cdn.test/viewer/config.json',settings,'cover.jpg');
assert.equal(scene.collision,'https://cdn.test/voxels/index.json');assert.equal(scene.settings.soundUrl,'https://cdn.test/viewer/audio/theme.mp3');assert.equal(scene.settings.background.skyboxUrl,'https://cdn.test/sky.hdr');assert.equal(scene.poster,'https://cdn.test/viewer/cover.jpg');assert.equal(JSON.stringify(settings),before);
assert.throws(()=>describeScene('https://cdn.test/model.sog','https://cdn.test/a.json',settings,'file:///secret'));
function request(){const handlers={};let resolve,reject;return {handlers,on:(n,f)=>handlers[n]=f,destroy(){this.destroyed=true},requestInStream:()=>new Promise((r,j)=>{resolve=r;reject=j}),status:n=>resolve(n),fail:()=>reject(new Error('network failure'))};}
(async()=>{
for(const responseFirst of [true,false]){
 current=request();const r=current,client=new ViewerCatalog(),progress=[];const task=client.download('https://cdn.test/a.sog',10,p=>progress.push(p));await Promise.resolve();
 if(responseFirst)r.status(200);
 r.handlers.dataReceiveProgress({totalSize:0,receiveSize:1});r.handlers.dataReceive(new Uint8Array([1,2]).buffer);
 r.handlers.dataReceiveProgress({totalSize:4,receiveSize:2});r.handlers.dataReceive(new Uint8Array([3,4]).buffer);r.handlers.dataEnd();
 if(!responseFirst)r.status(200);
 assert.deepEqual(Array.from(new Uint8Array(await task)),[1,2,3,4]);assert.deepEqual(progress,[-1,-1,50,100]);assert.equal(r.destroyed,true);
}
for(const mode of ['cancel','limit','http','network']){
 current=request();const r=current,client=new ViewerCatalog(),progress=[];const task=client.download('https://cdn.test/a.sog',2,p=>progress.push(p));const rejected=assert.rejects(task);await Promise.resolve();
 if(mode==='cancel')client.cancel();else if(mode==='limit')r.handlers.dataReceive(new Uint8Array(3).buffer);else if(mode==='http')r.status(404);else r.fail();
 await rejected;const n=progress.length;r.handlers.dataReceiveProgress({totalSize:100,receiveSize:99});r.handlers.dataEnd();assert.equal(progress.length,n,'late event cannot update UI');assert.equal(r.destroyed,true);
}
console.log('PASS config-relative URLs without mutation; known/unknown byte progress, both HTTP/end orderings, cancellation, size limit, HTTP/network failure, late-event isolation');
})().catch(e=>{console.error(e);process.exitCode=1});
