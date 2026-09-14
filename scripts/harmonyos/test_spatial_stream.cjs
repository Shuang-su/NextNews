const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
(async()=>{
 let callback,active=0,peak=0;const notified=[],saved=[],messages=[];
 const node={setCamera:()=>{},setTileRequestCallback:cb=>{callback=cb;},notifyTileReady:t=>{assert.ok(saved.includes(t.uri));notified.push(t.uri);},destroy:()=>{}};
 const box={exports:{},setTimeout,clearTimeout,console:{info:()=>{}},require:n=>n==='@kit.CoreFileKit'?{fileIo:{mkdir:async()=>{},rmdir:async()=>{}}}:n==='@kit.SpatialReconKit'?{spatialRender:{GSPlugin:{loadTiledGSNode:async(scene, resource)=>{assert.ok(resource.uri.endsWith("/stream.scene.json"));assert.ok(saved.includes("stream.scene.json"));return node;}}}}:{}};
 vm.runInNewContext(ts.transpileModule(fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/SpatialStream.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
 const session=new box.exports.SpatialStream(m=>messages.push(m));
 session.fetch=async()=>{active++;peak=Math.max(peak,active);await new Promise(setImmediate);active--;return new ArrayBuffer(16);};
 session.save=async(name)=>{await new Promise(setImmediate);saved.push(name);};
 await session.open('http://localhost/scene.json',{cacheDir:'/cache'},{root:{}},{});
 callback([{uri:'a.sog'},{uri:'a.sog'},{uri:'b.sog'},{uri:'c.sog'},{uri:'../secret.sog'},{uri:'https://other/x.sog'}]);
 for(let i=0;i<30&&notified.length<3;i++)await new Promise(setImmediate);
 assert.equal(peak,2);assert.deepEqual(notified.sort(),['a.sog','b.sog','c.sog']);assert.equal(saved.filter(x=>x==='a.sog').length,1);
 assert.ok(messages.some(m=>m.includes('不支持的瓦片路径')));
 callback([{uri:'a.sog'}]);assert.equal(notified.length,4,'cached file may notify again without another download');
 await session.dispose();assert.equal(callback,null);
 console.log('PASS native tile adapter: deduplication, two requests, write-before-notify, path rejection, cached notify, unregister');
})().catch(e=>{console.error(e);process.exitCode=1;});
