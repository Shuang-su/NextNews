const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const players=[],creations=[];
class Player {
 constructor(){this.state='idle';this.handlers={};this.calls=[];this.volume=1;}
 on(n,fn){this.handlers[n]=fn} off(n){delete this.handlers[n]}
 set url(v){this.source=v;this.change('initialized')}
 change(s){this.state=s;this.handlers.stateChange?.(s)}
 async prepare(){this.change('prepared')}
 async play(){this.calls.push('play');this.change('playing')}
 async pause(){this.calls.push('pause');this.change('paused')}
 async release(){this.calls.push('release');this.change('released')}
 setVolume(v){this.volume=v}
}
const box={exports:{},require:()=>({media:{createAVPlayer:()=>new Promise(r=>creations.push(()=>{const p=new Player;players.push(p);r(p)}))}})};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/ViewerSound.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
const flush=async()=>{for(let i=0;i<8;i++)await Promise.resolve()};
(async()=>{
 const errors=[],sound=new box.exports.ViewerSound(m=>errors.push(m));sound.configure('https://cdn/a.mp3');creations.shift()();await flush();const first=players[0];
 sound.shown(true);assert.equal(first.state,'prepared','no autoplay before gesture');sound.gesture();await flush();assert.equal(first.state,'playing');assert.equal(first.loop,true);
 sound.level(.4,true);assert.equal(first.volume,0);sound.level(.4,false);assert.equal(first.volume,.4);
 sound.active(false);await flush();assert.equal(first.state,'paused');sound.active(true);await flush();assert.equal(first.state,'playing');
 sound.configure('https://cdn/b.mp3');assert.equal(first.state,'released');sound.configure('https://cdn/c.mp3');creations.shift()();await flush();assert.equal(players[1].state,'released','late creation cannot attach');creations.shift()();await flush();const last=players[2];assert.equal(last.state,'prepared','scene change rearms first-presentation gate');sound.shown(true);await flush();assert.equal(last.state,'playing');assert.equal(last.volume,.4);
 sound.active(false);sound.active(true);await flush();assert.equal(last.state,'playing','rapid lifecycle inversion settles at latest intent');sound.clear();assert.equal(last.state,'released');assert.equal(Object.keys(last.handlers).length,0);sound.configure('file:///secret');assert.match(errors.at(-1),/HTTPS/);
 console.log('PASS audio gesture/presentation gate, mute/volume, foreground, replacement, late creation, rapid intent changes and release');
})().catch(e=>{console.error(e);process.exitCode=1});
