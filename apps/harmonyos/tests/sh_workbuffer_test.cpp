#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include "sh_workbuffer.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
int main(){try{
 CGLPixelFormatAttribute attrs[]={kCGLPFAOpenGLProfile,(CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,kCGLPFAAccelerated,(CGLPixelFormatAttribute)0};
 CGLPixelFormatObj format;GLint count;CGLContextObj context;
 if(CGLChoosePixelFormat(attrs,&format,&count)||CGLCreateContext(format,nullptr,&context))throw std::runtime_error("Desktop GPU unavailable");
 CGLDestroyPixelFormat(format);CGLSetCurrentContext(context);
 GLuint positions,fbo;glGenTextures(1,&positions);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,positions);
 std::vector<float> data(4096*4);data[1]=1;data[16+1]=-1;
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,4096,1,0,GL_RGBA,GL_FLOAT,data.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glGenFramebuffers(1,&fbo);
 splat::ShTexture coefficients;splat::ShWorkbuffer work;
 splat::View view{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},.1f,100.f,.414f};
 for(bool compressed:{false,true}){
  auto scene=std::make_shared<splat::Scene>();scene->points.resize(2);scene->shDegree=1;
  if(compressed){
   auto sh=std::make_shared<splat::SogHarmonics>();sh->degree=1;sh->width=192;sh->height=1;sh->centroids.resize(192*4);sh->books[1][1]=.2f;sh->books[2][1]=-.3f;
   for(int c=0;c<3;c++){sh->centroids[c]=1;sh->centroids[12+c]=2;}
   scene->sogHarmonics=sh;scene->shLabels={{0,0},{1,0}};
  }else{
   scene->harmonics.resize(2);for(int c=0;c<3;c++){scene->harmonics[0][c]=scene->harmonics[1][c]=.5f;scene->harmonics[0][3+c]=.2f;scene->harmonics[1][3+c]=-.3f;}
  }
  while(!coefficients.Prepare(scene,4*1024*1024)){}
  for(int flip:{0,1}){
   view.matrix[13]=flip?-2:0;
   if(!work.Prepare(scene,view,3,positions,coefficients))throw std::runtime_error("Workbuffer unavailable");
   GLint program=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);work.Bind(program,true);
   glActiveTexture(GL_TEXTURE9);GLint color;glGetIntegerv(GL_TEXTURE_BINDING_2D,&color);
   glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
   uint32_t bits[8];glReadPixels(0,0,2,1,GL_RGBA_INTEGER,GL_UNSIGNED_INT,bits);float actual[8];std::memcpy(actual,bits,sizeof(bits));
   for(int c=0;c<3;c++){
    double a=.5-.4886025119*.2*(flip?-1:1),b=.5-.4886025119*-.3*-1;
    if(std::abs(actual[c]-a)>2e-6||std::abs(actual[4+c]-b)>2e-6)throw std::runtime_error("Workbuffer source/eye mismatch");
   }
   const size_t updates=work.Updates();
   if(!work.Prepare(scene,view,3,positions,coefficients)||work.Updates()!=updates)throw std::runtime_error("Unchanged camera recomputed colors");
  }
  if(work.Prepare(scene,view,0,positions,coefficients))throw std::runtime_error("SH0 did not bypass workbuffer");
  work.Destroy();coefficients.Destroy();view.matrix[13]=0;
 }
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU error");
 glDeleteTextures(1,&positions);glDeleteFramebuffers(1,&fbo);CGLSetCurrentContext(nullptr);CGLDestroyContext(context);
 std::cout<<"PASS production SH workbuffer: float/compressed, distinct Gaussian identity, camera movement, reuse, SH0 bypass, recreation\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
