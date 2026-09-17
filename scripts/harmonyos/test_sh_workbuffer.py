"""Exercise production SH uploads and color workbuffer on desktop GPU.

ES texture storage is shimmed to desktop allocation; this is not phone evidence.
"""
import subprocess
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[2]
core = root / 'apps/harmonyos/entry/src/main/cpp'
with tempfile.TemporaryDirectory(prefix='nextnews-sh-workbuffer-') as temporary:
    target = Path(temporary)
    (target / 'GLES3').mkdir()
    (target / 'GLES3/gl3.h').write_text('''#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
inline void glTexStorage2D(GLenum target,GLsizei,GLenum format,GLsizei w,GLsizei h){
 GLenum external=GL_RGBA,type=GL_FLOAT;
 if(format==GL_RG32UI){external=GL_RG_INTEGER;type=GL_UNSIGNED_INT;}
 else if(format==GL_RGBA32UI){external=GL_RGBA_INTEGER;type=GL_UNSIGNED_INT;}
 else if(format==GL_RGBA8UI){external=GL_RGBA_INTEGER;type=GL_UNSIGNED_BYTE;}
 else if(format==GL_RG32F)external=GL_RG;
 glTexImage2D(target,0,format,w,h,0,external,type,nullptr);
}
''')
    (target / 'sh_workbuffer.cpp').write_text((core / 'sh_workbuffer.cpp').read_text().replace('#version 300 es', '#version 410 core'))
    executable = target / 'test'
    subprocess.run(['xcrun', 'clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-I', str(target), '-I', str(core), str(target / 'sh_workbuffer.cpp'),
                    str(core / 'sh_texture.cpp'), str(root / 'apps/harmonyos/tests/sh_workbuffer_test.cpp'),
                    '-framework', 'OpenGL', '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
