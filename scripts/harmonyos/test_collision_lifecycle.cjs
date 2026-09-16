// Exercise the actual ArkTS loader with a controlled transport and real temp files.
const fs=require('fs'),fsp=fs.promises,os=require('os'),path=require('path'),crypto=require('crypto'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
function load(name,deps={}){const box={exports:{},require:n=>deps[n]||{},ArrayBuffer,Uint8Array,Promise,Date,Set,Map,Error};vm.runInNewContext(ts.transpileModule(fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/'+name+'.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);return box.exports;}
const sleep=()=>new Promise(r=>setTimeout(r,2));
async function until(test){for(let i=0;i<500;i++){if(test())return;await sleep();}throw Error('Timed out waiting for controlled loader');}
(async()=>{
 const dir=await fsp.mkdtemp(path.join(os.tmpdir(),'nextnews-cache-'));
 try{
  const handles=new Map(),requests=[],messages=[];let generation=0,ids=[],decodeCalls=0,rejectDecode=false,writeFailure=false;
  const io={OpenMode:{CREATE:64,WRITE_ONLY:1,TRUNC:512},accessSync:fs.existsSync,mkdir:p=>fsp.mkdir(p),listFile:p=>fsp.readdir(p),stat:async p=>{const s=await fsp.stat(p);return {size:s.size,mtime:s.mtimeMs/1000};},readText:p=>fsp.readFile(p,'utf8'),unlink:p=>fsp.unlink(p),rename:(a,b)=>fsp.rename(a,b),
   open:async p=>{const f=await fsp.open(p,'w');handles.set(f.fd,f);return {fd:f.fd};},write:async(fd,data)=>{if(writeFailure)throw Error('simulated disk full');return (await handles.get(fd).write(Buffer.from(data))).bytesWritten;},close:async f=>{await handles.get(f.fd).close();handles.delete(f.fd);}};
  const native={collisionClear:()=>{ids=[];return ++generation;},collisionPause(){},collisionSelect(){},collisionStatus:()=>({ids:ids.slice(),bytes:ids.length*100}),collisionLoadMesh:async(g,id,file)=>{decodeCalls++;assert.equal(g,generation);assert.equal(await fsp.readFile(file,'utf8'),'valid-mesh');if(rejectDecode)throw Error('Invalid GLB collision');ids.push(id);return[];}};
  const http={HttpDataType:{ARRAY_BUFFER:1},createHttp(){const request={destroyed:false,destroy(){this.destroyed=true;},request(address){return new Promise(resolve=>{requests.push({address,request,finish(text){const bytes=Buffer.from(text);resolve({responseCode:200,result:bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength)});}});});}};return request;}};
  const util={TextEncoder:class{encodeInto(text){return new Uint8Array(Buffer.from(text));}}};
  const cryptoFramework={createMd(){let text;return {async update({data}){text=data;},async digest(){return {data:new Uint8Array(crypto.createHash('sha256').update(text).digest())};}};}};
  const pool=load('ViewerRequests').viewerRequests;
  const {ViewerCollision}=load('ViewerCollision',{'@kit.CoreFileKit':{fileIo:io},'@kit.NetworkKit':{http},'@kit.ArkTS':{util,url:{URL}},'@kit.CryptoArchitectureKit':{cryptoFramework},'libsplat.so':{default:native},'./ViewerRequests':{viewerRequests:pool}});
  // A response finishing after pause+resume cannot publish as current work.
  const c=new ViewerCollision(m=>messages.push(m));await c.open('https://example.test/room.glb',dir,[0,1,0]);await until(()=>requests.length===1);
  c.pause(true);c.pause(false);requests[0].finish('obsolete-response');await until(()=>requests.length===2);assert.equal(decodeCalls,0);requests[1].finish('valid-mesh');await until(()=>ids.length===1);
  await until(()=>c.inflight.size===0);assert.equal(decodeCalls,1);assert.equal(messages.filter(m=>m.includes('已缓存')).length,1);c.stop();
  // Parsing failure removes the poisoned file; retry fetches it anew.
  rejectDecode=true;const d=new ViewerCollision(m=>messages.push(m));await d.open('https://example.test/bad.glb',dir,[0,1,0]);await until(()=>requests.length===3);requests[2].finish('valid-mesh');await until(()=>d.failed.size===1&&d.inflight.size===0);
  const failedPath=d.paths.get('https://example.test/bad.glb');assert(!fs.existsSync(failedPath));rejectDecode=false;d.retry();await until(()=>requests.length===4);requests[3].finish('valid-mesh');await until(()=>ids.length===1);await until(()=>d.inflight.size===0);d.stop();
  // Partial files are cleaned and the disk publication lock is released on error.
  writeFailure=true;const e=new ViewerCollision(()=>{});await e.open('https://example.test/disk.glb',dir,[0,1,0]);await until(()=>requests.length===5);requests[4].finish('valid-mesh');await until(()=>e.failed.size===1&&e.inflight.size===0);
  assert(!(await fsp.readdir(path.join(dir,'viewer-collision-v1'))).some(n=>n.endsWith('.part')));writeFailure=false;e.retry();await until(()=>requests.length===6);requests[5].finish('valid-mesh');await until(()=>ids.length===1);await until(()=>e.inflight.size===0);e.stop();
  // Invalid manifest cannot remain cached and the retry button restarts opening.
  const f=new ViewerCollision(()=>{});const opening=f.open('https://example.test/tiles.json',dir,[0,1,0]);await until(()=>requests.length===7);requests[6].finish('{');await assert.rejects(opening);assert(!fs.existsSync(f.paths.get('https://example.test/tiles.json')));
  f.retry();await until(()=>requests.length===8);f.stop();requests[7].finish('{}');await until(()=>!f.reopening);assert.equal(handles.size,0);
  console.log('PASS pause/resume stale responses, poisoned-cache retry, partial-file cleanup, write lock release, invalid-manifest restart and stop');
 }finally{await fsp.rm(dir,{recursive:true,force:true});}
})().catch(error=>{console.error(error);process.exitCode=1;});
