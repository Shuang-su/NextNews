const fs=require('fs'),fsp=fs.promises,os=require('os'),path=require('path'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const box={exports:{},require:()=>({fileIo:{listFile:fsp.readdir,stat:fsp.stat,unlink:fsp.unlink}})};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/ViewerSkyCache.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
(async()=>{
 const dir=await fsp.mkdtemp(path.join(os.tmpdir(),'sky-lru-'));
 try {
  const paths=['a','b','c'].map(c=>path.join(dir,'sky-'+c.repeat(64)+'.image'));
  for(let i=0;i<3;i++){await fsp.writeFile(paths[i],new Uint8Array(10));await fsp.utimes(paths[i],new Date(i*1000),new Date(i*1000));}
  const original=path.join(dir,'original.hdr');await fsp.writeFile(original,'original');
  box.exports.retainSkyImage(paths[0]);box.exports.retainSkyImage(paths[0]);
  await box.exports.pruneSkyImages(dir,20);assert.ok(fs.existsSync(paths[0]));assert.ok(!fs.existsSync(paths[1]));assert.ok(fs.existsSync(paths[2]));
  box.exports.releaseSkyImage(paths[0]);await box.exports.pruneSkyImages(dir,10);assert.ok(fs.existsSync(paths[0]));assert.ok(!fs.existsSync(paths[2]));
  box.exports.releaseSkyImage(paths[0]);await box.exports.pruneSkyImages(dir,0);assert.ok(!fs.existsSync(paths[0]));assert.equal(await fsp.readFile(original,'utf8'),'original');
  console.log('PASS actual byte limit, oldest-first eviction, double reference protection, release and original preservation');
 }finally{await fsp.rm(dir,{recursive:true,force:true})}
})().catch(e=>{console.error(e);process.exitCode=1});
