// Pure input-controller regression checks; the HAP build verifies ArkTS compatibility.
const fs = require('fs');
const assert = require('node:assert/strict');
const ts = require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const vm = require('vm');
const source = fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/OrbitControls.ets', 'utf8');
const box = { exports: {} };
vm.runInNewContext(ts.transpileModule(source, { compilerOptions: { module: ts.ModuleKind.CommonJS } }).outputText, box);
const Controls = box.exports.OrbitControls;
const point = (id,x,y) => ({id,x,y});
const close = (a,b) => assert.ok(Math.abs(a-b)<1e-6, `${a} != ${b}`);
const c = new Controls(); c.resize(400,800);
c.touch([point(1,100,100)],false); c.touch([point(1,140,120)],false);
assert.ok(c.target[0]<0 && c.target[1]>0);
const previous = Array.from(c.target);
c.touch([point(1,140,120),point(2,240,120)],false);
assert.deepEqual(Array.from(c.target),previous); // no jump when a finger is added
c.touch([point(2,280,140),point(1,120,140)],false); // reordered pointers, spread and translate
close(c.target[2],100/160); assert.ok(c.target[3]<0 && c.target[4]>0);
const beforeRelease=Array.from(c.target);
c.touch([point(2,280,140)],false); assert.deepEqual(Array.from(c.target),beforeRelease);
c.cancel(); c.touch([point(3,300,300)],false); assert.deepEqual(Array.from(c.target),beforeRelease);
const a=new Controls(), b=new Controls(); a.target[0]=b.target[0]=1;
a.tick(32); b.tick(16); b.tick(16); close(a.current[0],b.current[0]);
for(let i=0;i<100;i++) a.tick(32); close(a.current[0],1); assert.equal(a.tick(32),false);
c.zoom(1e10); close(c.target[2],20); c.zoom(1e-10); close(c.target[2],.05);
c.reset(); assert.deepEqual(Array.from(c.current),[0,0,1,0,0,0]);
c.resize(400,800); c.pan(40,0); const pan=c.target[3];
c.resize(800,1600); c.reset(); c.pan(80,0); close(c.target[3],pan); // DPI-independent projection
c.reset(); c.current[0]=Math.PI/2; c.pan(10,0); assert.ok(c.target[5]>0); close(c.target[3],0);
c.reset(); c.resize(800,3200); close(c.current[2],2); c.resize(800,1600); close(c.current[2],1);
console.log('PASS orbit: finger transitions, simultaneous pan/pinch, ID ordering, damping, bounds, reset, viewport scaling, world pan');
const f = new Controls(); f.current=[.7,.3,1.4,.2,-.1,.3];f.target=f.current.slice();
const oldPose=Array.from(f.current);f.setFly(true);
const eye=Array.from(f.current.slice(3));f.rotate(30,10);
for(let i=0;i<100;i++)f.tick(32);
assert.deepEqual(Array.from(f.current.slice(3)),eye); // flight rotates in place
f.key('w',true); const z=f.target[5];f.tick(64);assert.notEqual(f.target[5],z);f.key('w',false);
f.cancel();assert.equal(f.keys.length,0);
const switcher=new Controls();switcher.current=oldPose.slice();switcher.target=oldPose.slice();switcher.setFly(true);switcher.setFly(false);
for(let i=0;i<6;i++)close(switcher.current[i],oldPose[i]);
console.log('PASS flight: in-place rotation, held-key movement, cancellation and mode roundtrip');
