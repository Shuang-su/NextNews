const fs=require('fs'), assert=require('node:assert/strict'),vm=require('vm');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const source=fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/StreamSession.ets','utf8');
const box={exports:{},require:()=>({})};
vm.runInNewContext(ts.transpileModule(source,{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
const validate=box.exports.validateManifest;
const base={version:1,bounds:[0,0,0,1],chunks:[{file:'chunk-0000.ply',count:100,bytes:6000,bounds:[0,0,0,1]}]};
validate(base);
for(const mutate of [m=>m.bounds[0]=NaN,m=>m.bounds[3]=0,m=>m.chunks[0].file='../secret',m=>m.chunks[0].count=32769,m=>m.chunks[0].bytes=2097153,m=>m.chunks.push(m.chunks[0]),m=>m.chunks=null]){
 const m=JSON.parse(JSON.stringify(base));mutate(m);assert.throws(()=>validate(m));
}
console.log('PASS stream manifest: bounds, traversal, chunk count, byte budget, duplicate paths, invalid arrays');
