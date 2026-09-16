#!/usr/bin/env python3
"""Portable decoder regression; does not claim GPU or phone rendering."""
import pathlib, tempfile, struct, zlib, subprocess
root = pathlib.Path(__file__).resolve().parents[2]
cpp = root / 'apps/harmonyos/entry/src/main/cpp'
def png(w,h,rows):
    def chunk(kind,data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')
with tempfile.TemporaryDirectory() as tmp:
    d=pathlib.Path(tmp); exe=d/'test'
    subprocess.run(['xcrun','clang++','-std=c++17','-fsanitize=address,undefined','-I',str(cpp),str(root/'apps/harmonyos/tests/sky_image_test.cpp'),str(cpp/'sky_image.cpp'),'-o',str(exe)],check=True)
    valid=png(4,2,(b'\0'+bytes([255,0,0])*4)*2)
    hdr=b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 4\n'+bytes([128,0,0,129])*8
    cases=[('valid.png',valid,False),('valid.hdr',hdr,False),('broken',b'broken',True),('truncated.png',valid[:40],True),('square.png',png(2,2,(b'\0'+bytes([255,0,0])*2)*2),True),('oversize.png',png(8192,4096,b''),True),('overflow.hdr',hdr[:-32]+bytes([255,255,255,255])*8,True)]
    for name,data,reject in cases:
        path=d/name;path.write_bytes(data)
        subprocess.run([str(exe),str(path)]+(['reject'] if reject else []),check=True)
print('sky image: valid PNG/HDR, corrupt/truncated/non-panorama/oversize/HDR overflow passed')
