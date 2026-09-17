#!/usr/bin/env python3
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile
from prepare_model import FIELDS

ROOT=Path(__file__).resolve().parents[2]
CORE=ROOT/'apps/harmonyos/entry/src/main/cpp'
OUTPUT=ROOT/'artifacts/harmonyos/core-test'
OUTPUT.parent.mkdir(parents=True,exist_ok=True)
subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',
                '-I',str(CORE),str(CORE/'splat.cpp'),str(ROOT/'apps/harmonyos/tests/core_test.cpp'),'-o',str(OUTPUT)],check=True)

def model(values,fields=FIELDS,count=1,extra=''):
    return ('ply\nformat binary_little_endian 1.0\nelement vertex '+str(count)+'\n'+
            ''.join('property float '+f+'\n' for f in fields)+extra+'end_header\n').encode()+struct.pack('<'+'f'*len(values),*values)

v=[0,0,0,0,0,0,0,math.log(2),0,math.log(3),math.sqrt(.5),0,0,math.sqrt(.5)]
fixtures={'analytic':('analytic',model(v)), 'truncated':('invalid',model(v)[:-1]),
          'missing':('invalid',model(v[:-1],FIELDS[:-1])),
          'pointcloud':('invalid',model([0,0,0],FIELDS[:3])),
          'zero_quaternion':('invalid',model(v[:10]+[0]*4)),
          'nan':('invalid',model([float('nan')]+v[1:])),
          'inf':('invalid',model(v[:7]+[float('inf')]+v[8:])),
          'scale_overflow':('invalid',model(v[:7]+[1000]+v[8:])),
          'too_many':('invalid',model(v,count=4000001)),
          'negative_count':('invalid',model(v,count=-1)),
          'high_order_sh':('invalid',model(v+[0],FIELDS+['f_rest_0'])),
          'duplicate':('invalid',model(v+[0],FIELDS+['x'])),
          'extra_payload':('invalid',model(v)+b'extra'),
          'wrong_format':('invalid',model(v).replace(b'binary_little_endian',b'binary_big_endian'))}
for degree,n in [(1,9),(2,24),(3,45)]:
    fields=FIELDS+['f_rest_'+str(i) for i in range(n)]
    values=v+list(range(1,n+1))
    fixtures['sh'+str(degree)]=('sh'+str(degree),model(values,fields))
    fixtures['sh_reordered'+str(degree)]=('sh'+str(degree),model(list(reversed(values)),list(reversed(fields))))
    fixtures['sh_gap'+str(degree)]=('invalid',model(values,fields[:-1]+['f_rest_'+str(n)]))
    fixtures['sh_nan'+str(degree)]=('invalid',model(values[:-1]+[float('nan')],fields))
with tempfile.TemporaryDirectory(prefix='nextnews-tests-') as folder:
    for name,(mode,data) in fixtures.items():
        path=Path(folder)/(name+'.ply');path.write_bytes(data)
        print(name,flush=True);subprocess.run([str(OUTPUT),mode,str(path)],check=True,timeout=30)
    huge=Path(folder)/'oversize.ply'
    with huge.open('wb') as f:f.truncate(128*1024*1024+1)
    subprocess.run([str(OUTPUT),'invalid',str(huge)],check=True,timeout=30)
for model_path in (ROOT/'apps/harmonyos/entry/src/main/resources/rawfile/models').glob('*.ply'):
    subprocess.run([str(OUTPUT),'valid',str(model_path)],check=True,timeout=30)
print('All native core fixtures and staged models passed with ASan + UBSan.')
