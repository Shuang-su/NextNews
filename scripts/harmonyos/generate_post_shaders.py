import argparse
from pathlib import Path
parser = argparse.ArgumentParser(description="Regenerate native effects from the pinned MetaFlow PlayCanvas package")
parser.add_argument("engine", type=Path, help="unpacked playcanvas 2.21.3 package directory")
args = parser.parse_args()
import json
if json.loads((args.engine / "package.json").read_text())["version"] != "2.21.3":
    raise SystemExit("Expected PlayCanvas 2.21.3; refusing a silent reference upgrade")
root=args.engine/'build/playcanvas/src/scene/shader-lib/glsl/chunks'
def chunk(p):return (root/p).read_text().split('`')[1].replace('texture2DLod(', 'textureLod(').replace('texture2D(', 'texture(').replace('texture2D (','texture (').replace('varying vec2 uv0;', 'in vec2 uv0;').replace('gl_FragColor','outColor')
header='#version 300 es\nprecision highp float;\nout vec4 outColor;\n'
common='in vec2 uv0;\nuniform sampler2D sceneTexture;\nuniform vec2 sceneTextureInvRes;\nuniform float settings[22];\n'
# PlayCanvas tone mapping and post-processing equations, MIT, see THIRD_PARTY_NOTICES.
body=''
for i,name in enumerate(['None','Linear','Filmic','Hejl','Aces','Aces2','Neutral']):
 body+=chunk('common/frag/tonemapping/tonemapping'+name+'.js').replace('toneMap(',f'toneMap{i}(').replace('getExposure()','1.0')+'\n'
tones=body
for name,define in [('cas','CAS'),('fringing','FRINGING'),('grading','GRADING'),('vignette','VIGNETTE')]:
 body+='#define '+define+'\n'
 if name=='cas':body+='#define CAS_HDR\n'
 body+=chunk('render-pass/frag/compose/compose-'+name+'.js')+'\n'
# MetaFlow overrides gsplatOutputVS to write gamma colors directly. Preserve its compose input.
body=body.replace('return c / (1.0 + maxComponent(c.r, c.g, c.b));', 'return settings[1]>0.0 ? c / (1.0 + maxComponent(c.r, c.g, c.b)) : c;').replace('return c / max(1.0 - maxComponent(c.r, c.g, c.b), 1e-4);', 'return settings[1]>0.0 ? c / max(1.0 - maxComponent(c.r, c.g, c.b), 1e-4) : c;')
main='''
uniform sampler2D bloomTexture;
void main(){
 vec3 result=texture(sceneTexture,uv0).rgb;
 if(settings[2]>0.0)result=applyCas(result,uv0,sharpness);
 if(settings[3]>0.0)result=applyFringing(result,uv0);
 if(settings[4]>0.0 && settings[1]>0.0)result+=texture(bloomTexture,uv0).rgb*settings[5];
 if(settings[7]>0.0)result=applyGrading(result);
 result=max(result,vec3(0));
 int mode=int(settings[0]);
 if(mode==2)result=toneMap2(result);else if(mode==3)result=toneMap3(result);
 else if(mode==4)result=toneMap4(result);else if(mode==5)result=toneMap5(result);else if(mode==6)result=toneMap6(result);
 if(settings[14]>0.0)result=applyVignette(result,uv0);
 // MetaFlow configureCamera forces GAMMA_NONE on the final compose blit.
 outColor=vec4(max(result,vec3(0)),1);
}
'''
vertex='''#version 300 es
precision highp float;
out vec2 uv0;
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));uv0=p;gl_Position=vec4(p*2.0-1.0,0,1);}
'''
out='#pragma once\n// Adapted from PlayCanvas Engine (MIT); see docs/harmonyos/THIRD_PARTY_NOTICES.md.\nnamespace splat {\n'
for name,src in [('PostTone',tones),('PostVertex',vertex),('PostCompose',header+common+body+main),('PostDown',header+chunk('render-pass/frag/downsample.js')),('PostUp',header+chunk('render-pass/frag/upsample.js'))]:out+='inline const char* '+name+'=R"POST('+src+')POST";\n'
out+='}\n';Path('apps/harmonyos/entry/src/main/cpp/post_shaders.h').write_text(out)
