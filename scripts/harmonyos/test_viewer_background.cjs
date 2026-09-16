const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const calls=[],cameras=[];
const scene={environment:{},destroy(){},getResourceFactory(){return{async createCamera(){const c={};cameras.push(c);return c;}}}};
const deps={'@kit.ArkGraphics3D':{Scene:{getDefaultRenderContext:()=>({loadPlugin:async()=>true}),load:async()=>scene},EnvironmentBackgroundType:{BACKGROUND_NONE:0}},'@kit.SpatialReconKit':{spatialRender:{GSPlugin:{PLUGIN_ID:'test',loadGSNode:async()=>({})}}},'libsplat.so':{default:{background:(...color)=>calls.push(color)}},'./ViewerSettings':{}};
const box={exports:{},setTimeout,require:k=>deps[k]};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/ViewerBackend.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
(async()=>{
 assert.deepEqual(Array.from(box.exports.backgroundRGB([-1,1e300,.5])),[0,1,.5]);
 for(const invalid of [[],[NaN,0,0],[Infinity,0,0]])assert.throws(()=>box.exports.backgroundRGB(invalid));
 const gl=new box.exports.OpenGLBackend();gl.background([.1,.2,.3]);gl.background([0,0,0]);assert.deepEqual(calls,[[.1,.2,.3],[0,0,0]]);
 const h=new box.exports.HuaweiBackend(()=>{}),color=[.1,.2,.3];h.background(color);color[0]=1;
 await h.load({path:'test.ply',bounds:[0,0,0,1]});assert.deepEqual(JSON.parse(JSON.stringify(cameras[0].clearColor)),{r:.1,g:.2,b:.3,a:1});
 h.background([0,0,0]);assert.equal(cameras[0].clearColor.r,0);h.dispose();await h.load({path:'other.ply',bounds:[0,0,0,1]});assert.equal(cameras[1].clearColor.r,0);
 console.log('PASS background forwarding, pre-load retention, no borrowed color mutation, reset and Huawei camera recreation');
})().catch(e=>{console.error(e);process.exitCode=1});
