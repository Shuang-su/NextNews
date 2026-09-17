const fs=require('node:fs'),fsp=fs.promises,path=require('node:path'),os=require('node:os'),vm=require('node:vm'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const root='apps/harmonyos/entry/src/main/ets/pages/',cache={},handles=new Map();let fd=0;
const fileIo={OpenMode:{CREATE:1,READ_WRITE:2,TRUNC:4},listFile:fsp.readdir,mkdir:fsp.mkdir,rmdir:p=>fsp.rm(p,{recursive:true}),access:async p=>!!await fsp.stat(p).catch(()=>null),stat:fsp.stat,readText:p=>fsp.readFile(p,'utf8'),rename:fsp.rename,unlink:fsp.unlink,
 open:async p=>{const h=await fsp.open(p,'w');handles.set(++fd,h);return{fd}},close:async value=>{const id=typeof value==='number'?value:value.fd;await handles.get(id).close();handles.delete(id)},write:async(id,b)=>(await handles.get(id).write(new Uint8Array(b))).bytesWritten};
const kit={url:{URL},util:{TextDecoder:class{decodeWithStream(b){return new TextDecoder('utf-8',{fatal:true}).decode(b)}},TextEncoder:class{encodeInto(s){return new TextEncoder().encode(s)}}},taskpool:{execute:async(f,...a)=>f(...a)}};
const modules={'@kit.ArkTS':kit,'@kit.CoreFileKit':{fileIo},'@kit.CryptoArchitectureKit':{cryptoFramework:{createMd:()=>{const h=crypto.createHash('sha256');return{update:async b=>h.update(b.data),digest:async()=>({data:h.digest()})}}}},'libsplat.so':{default:{status:()=>({state:'ready'}),chunks:()=>{}}}};
function load(name){if(cache[name])return cache[name];const box={exports:{},require:n=>n.startsWith('./')?load(n.slice(2)):modules[n]||{},ArrayBuffer,Uint8Array,console,Date,Set,Map};cache[name]=box.exports;vm.runInNewContext(ts.transpileModule(fs.readFileSync(root+name+'.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS,experimentalDecorators:true}}).outputText,box);return box.exports;}
const Session=load('StreamSession').StreamSession;
(async()=>{
const tmp=await fsp.mkdtemp(path.join(os.tmpdir(),'nextnews-cache-'));
try {
 const manifest={version:1,bounds:[0,0,0,1],chunks:[{file:'chunk-0000.sog',source:'0_0/meta.json',count:2,bytes:0,bounds:[0,0,0,1]}]};
 const meta={version:2,count:2,means:{files:['l.webp','u.webp']},quats:{files:['q.webp']},scales:{files:['s.webp']},sh0:{files:['c.webp']}};
 let calls=[];function session(){const s=new Session(console.log);s.fetch=async url=>{calls.push(url);const value=url.endsWith('lod-meta.json')?JSON.stringify(manifest):url.endsWith('meta.json')?JSON.stringify(meta):'texture';return new TextEncoder().encode(value).buffer;};return s;}
 const first=session();await first.open('https://cdn.example/stream/lod-meta.json',tmp);await first.pump([0,0,1,0,0,0],false,45,1);assert.equal(first.cache.size,1);assert.equal(calls.length,7);const file=first.cache.get('chunk-0000.sog');assert.ok(fs.existsSync(file));first.stop();
 calls=[];const warm=session();await warm.open('https://cdn.example/stream/lod-meta.json',tmp);assert.equal(warm.cache.size,1);assert.equal(warm.cache.get('chunk-0000.sog'),file);await warm.pump([0,0,1,0,0,0],false,45,1);assert.equal(calls.length,1);warm.stop();
 await fsp.unlink(path.join(path.dirname(file),'l.webp'));calls=[];const partial=session();await partial.open('https://cdn.example/stream/lod-meta.json',tmp);assert.equal(partial.cache.size,0);await partial.pump([0,0,1,0,0,0],false,45,1);assert.equal(partial.cache.size,1);assert.equal(calls.length,7);partial.stop();
 const cancel=session();await cancel.open('https://cdn.example/other/lod-meta.json',tmp);const original=cancel.fetch;cancel.fetch=async url=>{const b=await original(url);if(url.endsWith('l.webp'))cancel.stop();return b;};await cancel.pump([0,0,1,0,0,0],false,45,1);assert.equal(cancel.cache.size,0);assert.equal(fs.existsSync(path.join(cancel.directory,'chunk-0000.sog.parts/ready.json')),false);
 console.log('PASS CDN cold/warm cache, stable identity, incomplete cache repair, cancellation without publication');
 const Catalog=load('ViewerCatalog').ViewerCatalog;
 let requests=[];const single=()=>{const c=new Catalog();c.fetch=async address=>{requests.push(address);return new TextEncoder().encode(address.endsWith('.json')?JSON.stringify(meta):'texture').buffer;};return c;};
 const progress=[],one=single();const loose=await one.resource('https://cdn.example/single/source.scene.json',tmp,p=>progress.push(p));
 assert.equal(requests.length,6);assert.equal(path.basename(loose),'meta.json');assert.deepEqual(progress,[-1,100]);assert.ok(fs.existsSync(path.join(path.dirname(loose),'ready.json')));
 requests=[];assert.equal(await single().resource('https://cdn.example/single/source.scene.json',tmp),loose);assert.equal(requests.length,1);
 await fsp.unlink(path.join(path.dirname(loose),'q.webp'));requests=[];await single().resource('https://cdn.example/single/source.scene.json',tmp);assert.equal(requests.length,6);
 const canceled=single(),oldFetch=canceled.fetch;canceled.fetch=async address=>{const bytes=await oldFetch(address);if(address.endsWith('q.webp'))canceled.cancel();return bytes;};
 await assert.rejects(canceled.resource('https://cdn.example/canceled/meta.json',tmp));
 assert.equal(JSON.parse(await fsp.readFile(loose,'utf8')).means.files[0],'l.webp');
 meta.shN={files:['centroids.webp','labels.webp']};requests=[];
 const higher=await single().resource('https://cdn.example/sh/source.json',tmp);
 assert.equal(requests.length,8);assert.ok(fs.existsSync(path.join(path.dirname(higher),'labels.webp')));
 await fsp.unlink(path.join(path.dirname(higher),'centroids.webp'));requests=[];
 await single().resource('https://cdn.example/sh/source.json',tmp);assert.equal(requests.length,8);
 console.log('PASS seven-component SH SOG download and missing centroid repair');
 const concurrentPath=path.join(tmp,'concurrent.image');
 await Promise.all([single().writeResource(concurrentPath,new Uint8Array(8192).fill(17).buffer),single().writeResource(concurrentPath,new Uint8Array(4096).fill(33).buffer)]);
 const concurrent=await fsp.readFile(concurrentPath);assert.ok((concurrent.length===8192&&concurrent.every(v=>v===17))||(concurrent.length===4096&&concurrent.every(v=>v===33)));assert.equal((await fsp.readdir(tmp)).filter(n=>n.includes('.part-')).length,0);
 console.log('PASS concurrent cache writers publish one complete file and clean temporary copies');
 console.log('PASS single JSON SOG source classification-compatible cache, warm reuse, partial repair, cancellation and original metadata preservation');
} finally {await fsp.rm(tmp,{recursive:true,force:true});}
})().catch(e=>{console.error(e);process.exitCode=1});
