const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const ts=require('/Applications/DevEco-Studio.app/Contents/tools/ohpm/node_modules/typescript');
const dir='artifacts/harmonyos/cdn-discovery-20260916/',root='apps/harmonyos/entry/src/main/ets/pages/',cache={};
const ark={url:{URL},util:{TextDecoder:class{decodeWithStream(b){return new TextDecoder('utf-8',{fatal:true}).decode(b)}}}};
function load(name){if(cache[name])return cache[name];const box={exports:{},require:n=>n.startsWith('./')?load(n.slice(2)):n==='@kit.ArkTS'?ark:{},ArrayBuffer,Uint8Array,Map,Set};cache[name]=box.exports;vm.runInNewContext(ts.transpileModule(fs.readFileSync(root+name+'.ets','utf8'),{compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,box);return box.exports;}
const {ViewerCatalog,catalogUrl,validateProject}=load('ViewerCatalog');
(async()=>{
const catalog=JSON.parse(fs.readFileSync('docs/harmonyos/cdn-catalog-20260916.json')),client=new ViewerCatalog();let requests=[];
client.fetch=async address=>{requests.push(address);let text;if(address.includes('/detail/'))text=fs.readFileSync(dir+'detail-'+address.split('/').pop()+'.json','utf8');else if(address.includes('/list?'))text=fs.readFileSync(dir+'list-1.json','utf8');else{const p=catalog.projects.find(p=>p.lodSettings?.trim()===address.trim());assert.ok(p);text=fs.readFileSync(dir+'settings-'+p.modelId+'.json','utf8');}return new TextEncoder().encode(text).buffer;};
const list=await client.list(1,'天空');assert.equal(list.total,49);assert.ok(requests[0].includes('modelTitle=%E5%A4%A9%E7%A9%BA'));
for(const p of catalog.projects){const asset=await client.detail(p.modelId);assert.equal(asset.project.modelId,p.modelId);assert.equal(asset.content,(p.modelType==='lod'?p.lodMeta:p.modelFileUrl).trim());const n=requests.length;assert.equal(await client.detail(p.modelId),asset);assert.equal(requests.length,n);if(p.modelType==='lod')assert.equal(asset.collision,p.lodBin.trim());}
assert.equal((await client.detail('2095135795485462530')).settings.annotations.length,10);
assert.equal(catalogUrl(' https://example.org/a.json '),'https://example.org/a.json');
for(const value of ['file:///etc/passwd','https://user:password@example.org/a','javascript:foo','http://example.org/a','https://example.org/a#b'])assert.throws(()=>catalogUrl(value));
assert.throws(()=>validateProject({modelId:2095135795485462530,modelTitle:'x',modelType:'lod'}));
await assert.rejects(()=>client.detail('../secret'));await assert.rejects(()=>client.list(0,''));
const bad=new ViewerCatalog();bad.fetch=async()=>new TextEncoder().encode(JSON.stringify({code:200,data:{modelId:'123',modelTitle:'wrong',modelType:'default'}})).buffer;await assert.rejects(()=>bad.detail('456'));
console.log('PASS all 49 real API detail/config mappings, string IDs, detail reuse, search encoding, invalid URL/ID/response');
})().catch(e=>{console.error(e);process.exitCode=1});
