"""Create an original SH1 plane for front/back phone validation; never modifies source assets."""
import argparse
import math
import struct
from pathlib import Path
from prepare_model import FIELDS
parser=argparse.ArgumentParser()
parser.add_argument('output',type=Path)
args=parser.parse_args()
fields=FIELDS+['f_rest_'+str(i) for i in range(9)]
header=('ply\nformat binary_little_endian 1.0\ncomment NextNews synthetic SH1 direction test\nelement vertex 256\n'+''.join('property float '+f+'\n' for f in fields)+'end_header\n').encode()
rows=[]
for y in range(16):
    for x in range(16):
        # Camera-to-point +Z yields orange; -Z yields blue; SH0 is neutral gray.
        rows.append(struct.pack('<23f',(x-7.5)*.07,(y-7.5)*.07,0,0,0,0,3,
            math.log(.045),math.log(.045),math.log(.045),1,0,0,0,
            0,1,0,0,0,0,0,-1,0))
with args.output.open('xb') as output:output.write(header+b''.join(rows))
