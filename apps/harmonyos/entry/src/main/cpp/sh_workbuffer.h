#pragma once
#include "sh_texture.h"
namespace splat {
// Evaluate direction-dependent color once per Gaussian. Sorting only changes
// draw addresses; cached colors remain indexed by source Gaussian identity.
class ShWorkbuffer {
public:
 bool Prepare(const std::shared_ptr<const Scene>& scene,const View& view,int degree,
              GLuint positions,const ShTexture& coefficients);
 void Bind(GLuint program,bool enabled)const;
 void Destroy();
 size_t Updates()const{return updates_;}
 size_t Bytes()const{return size_t(rows_)*4096*16;}
private:
 size_t updates_=0;
 GLuint program_=0,vao_=0,fbo_=0,texture_=0;
 int rows_=0,degree_=-1;
 bool compressed_=false;
 std::shared_ptr<const Scene> scene_;
 std::array<float,3> eye_{};
};
}
