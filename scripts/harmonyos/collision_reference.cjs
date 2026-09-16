// Differential oracle: executes the pinned original implementation without editing it.
const fs=require('fs'),vm=require('vm'),ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
function read(name,deps={}){const box={exports:{},require:n=>deps[n]};vm.runInNewContext(ts.transpileModule(fs.readFileSync('.local/supersplat-viewer-reference/src/collision/'+name+'.ts','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);return box.exports;}
const core=read('collision'),{VoxelCollision}=read('voxel-collision',{'./collision':core});
const [meta,bin,query]=process.argv.slice(2),m=JSON.parse(fs.readFileSync(meta)),b=fs.readFileSync(bin),v=new Uint32Array(b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength));
const c=new VoxelCollision(m,v.slice(0,m.nodeCount),v.slice(m.nodeCount)),out=[];
for(const q of JSON.parse(fs.readFileSync(query))){let hit;if(q.op==='ray')hit=c.queryRay(...q.p,...q.d,q.distance);else {const p={x:0,y:0,z:0};hit=(q.op==='sphere'?c.querySphere(...q.p,q.radius,p):c.queryCapsule(...q.p,q.half,q.radius,p))?p:null;}out.push(hit?[hit.x,hit.y,hit.z]:null);}
console.log(JSON.stringify(out));
