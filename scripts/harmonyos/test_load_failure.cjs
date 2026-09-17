const fs=require('fs'),vm=require('vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const box={exports:{}};
vm.runInNewContext(ts.transpileModule(fs.readFileSync('apps/harmonyos/entry/src/main/ets/pages/ViewerLoadFailure.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);
const failure=box.exports.currentLoadFailure;
assert.equal(failure('error','decoder rejected',7,7),'decoder rejected');
assert.equal(failure('error','old failure',7,8),''); // a new scene downloads before native loading starts
assert.equal(failure('loading','old failure',7,7),''); // retry started
assert.equal(failure('ready','old failure',7,7),'');
assert.equal(failure('error','unowned',0,0),'');
assert.match(failure('error','High-order SH requires encoded SOG pages or compatibility mode',7,7),/SOG 编码纹理/);
console.log('PASS current native failure, stale scene isolation, retry and successful recovery');
