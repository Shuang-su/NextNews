#!/usr/bin/env python3
"""Inspect public SZTV 3DGS list/detail and JSON configuration, without model payload downloads.
Endpoints were observed in the public page's resource inventory. No login/token needed.
Raw responses remain in the chosen local output directory; models and original settings are unchanged.
"""
import argparse, concurrent.futures, datetime, hashlib, json, math, os, tempfile
from pathlib import Path
from urllib.parse import urlparse, urljoin
from urllib.request import Request, urlopen

API = 'https://sztvbmsapi.sztv.com.cn/api/gaussian/model/'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args(); a.out.mkdir(parents=True, exist_ok=True)

def fetch(url, path):
    if path.exists(): return json.loads(path.read_text())
    request_url = url.strip()
    if urlparse(request_url).scheme != 'https': raise ValueError('Only HTTPS JSON resources are inspected')
    with urlopen(Request(request_url, headers={'Accept':'application/json'}), timeout=30) as response:
        payload = response.read(32*1024*1024+1)
    if len(payload) > 32*1024*1024: raise ValueError('JSON exceeds 32 MiB inspection limit')
    result = json.loads(payload)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temporary:
        temporary.write(payload); temporary_path=Path(temporary.name)
    try: os.replace(temporary_path,path)
    finally: temporary_path.unlink(missing_ok=True)
    return result

first = fetch(API+'list?pageNum=1&pageSize=20', a.out/'list-1.json')
assert first.get('code', 200) == 200 and isinstance(first.get('rows'), list)
rows = first['rows'][:]
for page in range(2, math.ceil(first['total']/20)+1):
    rows.extend(fetch(API+f'list?pageNum={page}&pageSize=20', a.out/f'list-{page}.json')['rows'])
ids = list(dict.fromkeys(str(row['modelId']) for row in rows))

def inspect(model_id):
    result = {'modelId':model_id, 'detailUrl':API+'detail/'+model_id,
              'pageUrl':'https://www.sztv.com.cn/h5/3dgs/#/detail/'+model_id}
    try:
        response = fetch(result['detailUrl'], a.out/f'detail-{model_id}.json')
        if response.get('code') != 200: raise ValueError('Detail returned '+str(response.get('code')))
        d = response['data']
        for key in ['modelTitle','modelType','modelFileUrl','modelFileSize','lodSettings','lodMeta','lodBin']:
            result[key] = d.get(key)
        result['folderFilesType'] = type(d.get('folderFiles')).__name__
        result['jsonDataType'] = type(d.get('jsonData')).__name__
        settings = d.get('modelSetting')
        if isinstance(settings,str) and settings.strip():
            try: settings=json.loads(settings)
            except ValueError: result['inlineSettingsError']='not valid JSON'; settings=None
        result['settingsSource']='inline modelSetting' if isinstance(settings,dict) else None
        metadata = {}
        for field in ['lodSettings','lodMeta','lodBin']:
            url = d.get(field)
            if not isinstance(url,str) or not url: continue
            if not urlparse(url).path.lower().endswith('.json'):
                metadata[field]={'url':url,'status':'non-JSON, not downloaded'};continue
            name=hashlib.sha256(url.encode()).hexdigest()[:16]+'.json'
            try:
                value=fetch(url,a.out/name)
                metadata[field]={'url':url,'requestUrl':url.strip(),'trimmedWhitespace':url!=url.strip(),'status':'JSON ready','localFile':name,
                                 'keys':list(value) if isinstance(value,dict) else [],
                                 'bytes':(a.out/name).stat().st_size}
                if isinstance(value,dict):
                    for key in ['version','format','count','totalGaussians','totalSplats','lodLevels']:
                        if key in value and not isinstance(value[key],(dict,list)): metadata[field][key]=value[key]
                    for key in ['tiles','files','chunks','nodes','filenames']:
                        if isinstance(value.get(key),(list,dict)): metadata[field][key+'Count']=len(value[key])
                    if field=='lodMeta' and value.get('filenames'):
                        metadata[field]['filenameFormats']=sorted(set(urlparse(f).path.rsplit('.',1)[-1] for f in value['filenames']))
                        metadata[field]['environment']=value.get('environment')
                        first_chunk=value['filenames'][0]
                        if urlparse(first_chunk).path.endswith('.json'):
                            chunk_url=urljoin(url.strip(),first_chunk)
                            chunk_path=a.out/(hashlib.sha256(chunk_url.encode()).hexdigest()[:16]+'.json')
                            try:
                                chunk=fetch(chunk_url,chunk_path)
                                metadata[field]['chunkSample']={'url':chunk_url,'version':chunk.get('version'),
                                    'count':chunk.get('count'),'keys':list(chunk),'status':'JSON ready'}
                            except Exception as error: metadata[field]['chunkSample']={'url':chunk_url,'status':'error','error':str(error)}
                if field=='lodSettings': settings=value;result['settingsSource']='lodSettings URL'
            except Exception as error: metadata[field]={'url':url,'status':'error','error':str(error)}
        result['metadata']=metadata
        if isinstance(settings,dict):
            result['settings']={'version':settings.get('version','legacy'), 'keys':list(settings),
                'cameras':len(settings.get('cameras',[])), 'animations':len(settings.get('animTracks',[])),
                'annotations':len(settings.get('annotations',[])), 'startMode':settings.get('startMode'),
                'collisionUrl':settings.get('collisionUrl'), 'voxelUrl':settings.get('voxelUrl'),
                'voxelManifestUrl':settings.get('voxelManifestUrl'), 'soundUrl':settings.get('soundUrl')}
            (a.out/f'settings-{model_id}.json').write_text(json.dumps(settings,ensure_ascii=False,indent=2)+'\n')
        result['status']='detail ready'
    except Exception as error: result.update(status='error',error=str(error))
    return result

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: projects=list(pool.map(inspect,ids))
catalog={'capturedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),
         'listUrl':API+'list?pageNum=1&pageSize=20','total':first['total'],'inspected':len(projects),
         'payloadsDownloaded':False,'projects':projects}
(a.out/'catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'total':catalog['total'],'inspected':len(projects),'errors':[r for r in projects if r['status']=='error'],
    'projects':[{'id':r['modelId'],'title':r.get('modelTitle'),'type':r.get('modelType'),
                 'settings':r.get('settingsSource'),'metadata':r.get('metadata')} for r in projects]},ensure_ascii=False,indent=2))
