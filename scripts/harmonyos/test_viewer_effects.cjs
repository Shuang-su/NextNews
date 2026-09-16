const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const root='apps/harmonyos/entry/src/main/ets/pages/';let parameters;
function load(name,deps){const box={exports:{},require:n=>deps[n]};vm.runInNewContext(ts.transpileModule(fs.readFileSync(root+name+'.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);return box.exports;}
const settings=load('ViewerSettings',{'./ViewerSpline':{}}),backend=load('ViewerBackend',{'libsplat.so':{default:{effects:p=>parameters=Array.from(p)}},'@kit.ArkGraphics3D':{},'@kit.SpatialReconKit':{},'./ViewerSettings':settings});
const gl=new backend.OpenGLBackend();gl.effects();assert.equal(parameters.length,22);assert.equal(parameters[21],0);
const s=settings.createSettings();gl.effects(s);assert.equal(parameters[21],0,'baseline bypass allocates no target');
for(const key of ['sharpness','bloom','grading','vignette','fringing']){
 const c=settings.createSettings();c.postEffectSettings[key].enabled=true;if(key==='sharpness')c.postEffectSettings[key].amount=.8;
 gl.effects(c);assert.equal(parameters[21],1,key);assert.ok(parameters.every(Number.isFinite));
}
s.highPrecisionRendering=true;gl.effects(s);assert.equal(parameters[1],1);assert.equal(parameters[21],1);
for(const [i,mode]of ['none','linear','filmic','hejl','aces','aces2','neutral'].entries()){s.tonemapping=mode;gl.effects(s);assert.equal(parameters[0],i);}
assert.deepEqual(Array.from(gl.capabilities.effects),['sharpness','bloom','grading','vignette','fringing','tonemapping','rgba16f','skybox-equirect']);assert.equal(new backend.HuaweiBackend(()=>{}).capabilities.effects.length,0);
console.log('PASS per-effect packing, tone map IDs, baseline bypass, high precision and backend capability separation');
