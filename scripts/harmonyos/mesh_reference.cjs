// Executes the fixed upstream mesh queries directly, without editing the reference.
const fs=require('fs'),vm=require('vm'),ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
function read(name,deps={}){const box={exports:{},require:n=>deps[n]};vm.runInNewContext(ts.transpileModule(fs.readFileSync('.local/supersplat-viewer-reference/src/collision/'+name+'.ts','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);return box.exports;}
const core=read('collision'),{MeshCollision}=read('mesh-collision',{'./collision':core,'playcanvas':{}});
const data=JSON.parse(fs.readFileSync(process.argv[2])),queries=JSON.parse(fs.readFileSync(process.argv[3]));
const c=new MeshCollision(new Float32Array(data.positions.flat()),new Uint32Array(data.indices));
console.log(JSON.stringify(queries.map(q=>{let h;if(q.op==='ray')h=c.queryRay(...q.p,...q.d,q.distance);else{const p={x:0,y:0,z:0};h=(q.op==='sphere'?c.querySphere(...q.p,q.radius,p):c.queryCapsule(...q.p,q.half,q.radius,p))?p:null;}return h?[h.x,h.y,h.z]:null;})));
