#!/usr/bin/env python3
"""Create an explicitly synthetic GLB test floor/wall near the Huafa camera.
This tests the loader and physics; it is not a reconstructed collision asset.
"""
import argparse,json,struct,hashlib
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
positions=[[311,-83.12,285],[311,-83.12,325],[351,-83.12,325],[351,-83.12,285],[330,-83.12,285],[330,-79.12,285],[330,-79.12,325],[330,-83.12,325]]
indices=[0,1,2,0,2,3,4,5,6,4,6,7];v=struct.pack('<'+'f'*24,*(n for p in positions for n in p));b=v+struct.pack('<'+'H'*12,*indices)
j={'asset':{'version':'2.0','generator':'NextNews synthetic collision test'},'buffers':[{'byteLength':len(b)}],'bufferViews':[{'buffer':0,'byteOffset':0,'byteLength':len(v)},{'buffer':0,'byteOffset':len(v),'byteLength':24}],'accessors':[{'bufferView':0,'componentType':5126,'count':8,'type':'VEC3'},{'bufferView':1,'componentType':5123,'count':12,'type':'SCALAR'}],'meshes':[{'primitives':[{'attributes':{'POSITION':0},'indices':1}]}],'nodes':[{'mesh':0}],'scenes':[{'nodes':[0]}],'scene':0}
t=json.dumps(j).encode();t+=b' '*((-len(t))%4);b+=b'\0'*((-len(b))%4);body=struct.pack('<II',len(t),0x4e4f534a)+t+struct.pack('<II',len(b),0x004e4942)+b;data=struct.pack('<III',0x46546c67,2,len(body)+12)+body
(a.out/'room.glb').write_bytes(data);(a.out/'provenance.json').write_text(json.dumps({'source':'synthetic analytical floor/wall authored by NextNews','purpose':'GLB loader and collision regression only; not real-world collision alignment','floorY':-83.12,'wallX':330,'sha256':hashlib.sha256(data).hexdigest(),'license':'MIT'},indent=2)+'\n')
print(a.out/'room.glb')
