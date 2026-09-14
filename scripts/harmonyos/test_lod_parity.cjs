const fs=require('fs'),vm=require('node:vm'),assert=require('node:assert/strict'),cp=require('node:child_process');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const file=process.argv[2];if(!file)throw new Error('Pass a generated scene.json; build host tests first');
const manifest=JSON.parse(fs.readFileSync(file));const box={exports:{},require:()=>({})};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/StreamSession.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
const native=JSON.parse(cp.execFileSync('artifacts/harmonyos/lod_selection-test',[file],{maxBuffer:4*1024*1024}));
let trial=0;for(const yaw of [0,.846,-.65,Math.PI])for(const budget of [2000000,4000000]){
 const selected=box.exports.selectLods(manifest,[yaw,.056,1,.412,-.139,.446],true,budget,75,.5032405642394204);
 assert.deepEqual(Array.from(selected),native[trial++]);
 const count=selected.reduce((n,l,i)=>n+manifest.leaves[i].lods[l][2],0);assert.ok(count<=budget);
}
console.log(`PASS native/ArkTS exact selection parity: ${trial} poses/budgets, ${manifest.leaves.length} leaves`);
