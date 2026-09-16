const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const box={exports:{}};vm.runInNewContext(ts.transpileModule(fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/ViewerGamepad.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
const f=box.exports.gamepadFrame, near=(a,b)=>assert.ok(Math.abs(a-b)<1e-10,`${a} != ${b}`);
let x=f([1,-1,1,-1],.016,false,90);near(x.right,.064);near(x.forward,.064);near(x.yaw,-18*.016*Math.PI/180);near(x.pitch,x.yaw);
let first=f([0,0,1,1],.016,true,60);near(first.yaw,x.yaw*.5);near(first.pitch,-x.pitch*.5);
near(f([.001,0,0,0],.016,false,90).right,.000064); // no invented dead zone
near(f([1,0,0,0],.032,false,90).right,2*x.right);
assert(!f([0,0,0,0],.016,true,60).active);assert(!f([NaN,0,0,0],.016,true,60).active);assert(!f([1,0],.016,true,60).active);
near(f([1,0,0,0],10,false,90).right,.256);
console.log('PASS MetaFlow raw stick values, 4 units/s, 18 degrees/s, FOV scaling, frame-time scaling and malformed axes');

near(f([2,0,0,0],.016,false,90).right,.128); // two standard controllers sum without a second clamp
