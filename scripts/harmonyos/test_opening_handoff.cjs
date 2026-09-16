const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
// Exercise the actual page handoff independently of ArkUI rendering.
const source=fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/Index.ets','utf8');
const handoff=source.match(/  private beginVisibleIntro\(replay: boolean = false\): void \{[\s\S]*?\n  \}/)[0];
const event=source.match(/  private presented\(request: number\): void \{[\s\S]*?\n  \}/)[0];
const method=handoff+'\n'+event;
const calls=[];let presented=4;
const box={exports:{},nativeRender:{status:()=>({openingPresented:presented}),beginIntro:(...v)=>{calls.push(v);return true}}};
vm.runInNewContext(ts.transpileModule(`export class Page {${method}}`,{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
const page=new box.exports.Page();Object.assign(page,{sound:{shown(){}},opening:true,introAwaitingVisible:true,active:true,backendName:'OpenGL',openingRequest:4,viewer:{pose:()=>({target:[1,2,3]})}});
page.beginVisibleIntro();assert.equal(calls.length,0,'loading overlay must exit first');
page.opening=false;page.active=false;page.beginVisibleIntro();assert.equal(calls.length,0);assert.equal(page.introAwaitingVisible,true,'background does not consume handoff');
page.active=true;presented=3;page.beginVisibleIntro();assert.equal(calls.length,0,'stale scene cannot release new intro');
presented=4;page.beginVisibleIntro();assert.equal(JSON.stringify(calls),JSON.stringify([[4,1,2,3,[],0]]));
page.beginVisibleIntro();assert.equal(calls.length,1,'LOD and repeated resume cannot replay');
page.beginVisibleIntro(true);assert.equal(calls.length,2,'explicit replay is supported');
page.backendName='Huawei';page.beginVisibleIntro(true);assert.equal(calls.length,2);
console.log('PASS opening handoff: overlay exit, background/resume, stale scene, one-shot playback, world focus, explicit replay and backend isolation');

Object.assign(page,{visible:true,active:true,backendName:'OpenGL',opening:true,openingRequest:5});
page.presented(4);assert.equal(page.opening,true,'old download cannot hide new loading');
page.active=false;page.presented(5);assert.equal(page.opening,true,'background defers first-frame acknowledgement');
page.active=true;page.presented(5);assert.equal(page.opening,false,'native event hides loading immediately');
page.presented(5);assert.equal(page.opening,false,'duplicate does not reopen loading');
page.opening=true;page.visible=false;page.presented(5);assert.equal(page.opening,true,'destroyed page ignores queued events');
console.log('PASS native event acknowledgement and request/lifecycle isolation');

Object.assign(page,{visible:true,active:true,backendName:'OpenGL',opening:false,introAwaitingVisible:true,openingRequest:4});presented=4;
box.nativeRender.beginIntro=()=>false;page.beginVisibleIntro();assert.equal(page.introAwaitingVisible,true,'surface destroyed between acknowledgement and release retains pending handoff');
box.nativeRender.beginIntro=()=>true;page.presented(4);assert.equal(page.introAwaitingVisible,false,'new surface can release the held reveal without reopening loading');
