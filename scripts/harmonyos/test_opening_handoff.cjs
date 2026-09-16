const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
// Exercise the actual page handoff independently of ArkUI rendering.
const source=fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/Index.ets','utf8');
const method=source.match(/  private beginVisibleIntro\(replay: boolean = false\): void \{[\s\S]*?\n  \}/)[0];
const calls=[];let presented=4;
const box={exports:{},nativeRender:{status:()=>({openingPresented:presented}),beginIntro:(...v)=>calls.push(v)}};
vm.runInNewContext(ts.transpileModule(`export class Page {${method}}`,{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
const page=new box.exports.Page();Object.assign(page,{opening:true,introAwaitingVisible:true,active:true,backendName:'OpenGL',openingRequest:4,viewer:{pose:()=>({target:[1,2,3]})}});
page.beginVisibleIntro();assert.equal(calls.length,0,'loading overlay must exit first');
page.opening=false;page.active=false;page.beginVisibleIntro();assert.equal(calls.length,0);assert.equal(page.introAwaitingVisible,true,'background does not consume handoff');
page.active=true;presented=3;page.beginVisibleIntro();assert.equal(calls.length,0,'stale scene cannot release new intro');
presented=4;page.beginVisibleIntro();assert.deepEqual(calls,[[4,1,2,3]]);
page.beginVisibleIntro();assert.equal(calls.length,1,'LOD and repeated resume cannot replay');
page.beginVisibleIntro(true);assert.equal(calls.length,2,'explicit replay is supported');
page.backendName='Huawei';page.beginVisibleIntro(true);assert.equal(calls.length,2);
console.log('PASS opening handoff: overlay exit, background/resume, stale scene, one-shot playback, world focus, explicit replay and backend isolation');
