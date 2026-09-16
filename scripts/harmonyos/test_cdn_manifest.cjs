const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const root='apps/harmonyos/entry/src/main/ets/pages/';
const cache={};function load(name){if(cache[name])return cache[name];const box={exports:{},require:n=>n.startsWith('./')?load(n.slice(2)): {}};cache[name]=box.exports;vm.runInNewContext(ts.transpileModule(fs.readFileSync(root+name+'.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);return box.exports;}
const {parseLodManifest,sogComponents}=load('ViewerLodManifest'),{validateManifest}=load('StreamSession');
const catalog=JSON.parse(fs.readFileSync('docs/harmonyos/cdn-catalog-20260916.json'));
const dir='artifacts/harmonyos/cdn-discovery-20260916/';
function metadata(url){const hash=crypto.createHash('sha256').update(url.trim()).digest('hex').slice(0,16);return JSON.parse(fs.readFileSync(dir+hash+'.json'));}
let count=0;
for(const project of catalog.projects.filter(p=>p.modelType==='lod')){
 const raw=metadata(project.lodMeta),m=parseLodManifest(JSON.stringify(raw));validateManifest(m);
 assert.equal(m.chunks.length,raw.filenames.length+(raw.environment?1:0));
 let leaves=0;function walk(n){if(n.lods){const l=m.leaves[leaves++];for(let k=0;k<raw.lodLevels;k++){const r=n.lods[k];/* DFS uses stack; identity checked below */}}for(const child of n.children||[])walk(child);}walk(raw.tree);assert.equal(m.leaves.length,leaves);
 const ranges=new Map();function collect(n){if(n.lods)for(const [k,r]of Object.entries(n.lods))ranges.set(`${n.bound.min}:${n.bound.max}:${k}`,`${r.file}:${r.offset}:${r.count}`);for(const c of n.children||[])collect(c);}collect(raw.tree);
 for(const l of m.leaves)for(let k=0;k<m.levels;k++){const expected=ranges.get(`${l.aabb.slice(0,3)}:${l.aabb.slice(3)}:${k}`);assert.equal(l.lods[k].join(':'),expected||'-1:0:0');}
 console.log('PASS',project.modelTitle,m.chunks.length,m.leaves.length);count++;
}
assert.equal(count,14);
const sample={version:1,filenames:['0_0/meta.json'],lodLevels:1,tree:{bound:{min:[0,0,0],max:[1,1,1]},lods:{0:{file:0,offset:0,count:2}}}};
for(const change of [s=>s.filenames[0]='../meta.json',s=>s.tree.lods[0].offset=-1,s=>s.tree.lods[0].count=4000001,s=>s.tree.bound.max[0]=-1,s=>s.lodLevels=100]){const s=JSON.parse(JSON.stringify(sample));change(s);assert.throws(()=>parseLodManifest(JSON.stringify(s)));}
const meta={version:2,count:2,means:{files:['l.webp','u.webp']},quats:{files:['q.webp']},scales:{files:['s.webp']},sh0:{files:['c.webp']}};
assert.equal(sogComponents(meta,2).length,5);assert.throws(()=>sogComponents(meta,3));meta.quats.files=['../q.webp'];assert.throws(()=>sogComponents(meta,2));
console.log('PASS original file/range preservation and invalid source/component rejection');
