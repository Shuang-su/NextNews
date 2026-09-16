"""Regenerate SH polynomials from the pinned MetaFlow PlayCanvas package."""
import argparse
import json
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('engine',type=Path)
args=parser.parse_args()
assert json.loads((args.engine/'package.json').read_text())['version']=='2.21.3'
source=args.engine/'build/playcanvas/src/scene/shader-lib/glsl/chunks/gsplat/vert/gsplatEvalSH.js'
polynomial=source.read_text().split('`')[1]
adapter='uniform highp sampler2D shData;\nuniform int shBands;\nuniform bool shFlip;\nvec3 directionalColor(vec3 position,mat4 cameraView){\n vec4 packedSH[12];\n for(int i=0;i<12;i++){uint a=splatIndex*12u+uint(i);packedSH[i]=texelFetch(shData,ivec2(int(a%4096u),int(a/4096u)),0);}\n vec3 sh[15];\n for(int i=0;i<15;i++)for(int c=0;c<3;c++){int p=3+i*3+c;sh[i][c]=((shBands==1&&i>=3)||(shBands==2&&i>=8))?0.0:packedSH[p/4][p%4];}\n vec3 eye=-transpose(mat3(cameraView))*cameraView[3].xyz;\n vec3 delta=position-eye;vec3 dir=delta/max(length(delta),0.0000001);\n if(shFlip)dir.xy=-dir.xy;\n return max(vec3(0.0),packedSH[0].xyz+evalSH(sh,dir));\n}\n'
output=Path(__file__).resolve().parents[2]/'apps/harmonyos/entry/src/main/cpp/sh_shader.h'
output.write_text('#pragma once\nnamespace splat {\n// PlayCanvas 2.21.3 gsplatEvalSH, MIT; see THIRD_PARTY_NOTICES.md.\ninline const char* ShShader=R"SH(\n#define SH_BANDS 3\n'+polynomial+'\n'+adapter+')SH";\n}\n')
