#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include "hotspots.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(){try{
 CGLPixelFormatAttribute attributes[]={kCGLPFAOpenGLProfile,(CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,kCGLPFAAccelerated,(CGLPixelFormatAttribute)0};
 CGLPixelFormatObj format;GLint count;CGLContextObj context;
 if(CGLChoosePixelFormat(attributes,&format,&count)||CGLCreateContext(format,nullptr,&context))throw std::runtime_error("Desktop GPU unavailable");
 CGLDestroyPixelFormat(format);CGLSetCurrentContext(context);
 GLuint framebuffer,color,depth;glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
 glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
 glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,64,64);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
 if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Framebuffer incomplete");
 splat::Hotspots hotspots;auto data=std::make_shared<splat::HotspotData>();data->positions={0,0,-3};data->glyphs.assign(320*32,255);
 if(!hotspots.Prepare(data))throw std::runtime_error("Hotspot prepare failed");
 splat::View view{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},.1f,100.f,.41421356f};
 for(int tone:{0,4})for(int hover:{-1,0})for(bool occluded:{false,true}){
  glViewport(0,0,64,64);glDepthMask(GL_TRUE);glClearDepth(occluded?.1:1);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  splat::HotspotStyle style{true,hover,40};hotspots.Draw(view,64,64,style,false,tone);hotspots.Draw(view,64,64,style,true,tone);
  uint8_t actual[4];glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,actual);
  for(int c=0;c<3;c++){
   double v=hover<0?.8:c==0?1:c==1?.4:0;
   if(tone==4){double x=std::pow(v,2.2);v=std::pow((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14)+1e-7,1/2.2);}
   double expected=255*v*(occluded?.25:1);
   if(std::abs(actual[c]-expected)>2)throw std::runtime_error("Hotspot tone/depth/overlay mismatch");
  }
 }
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU error");
 hotspots.Destroy();glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&framebuffer);CGLSetCurrentContext(nullptr);CGLDestroyContext(context);
 std::cout<<"PASS production hotspot draws: default/ACES, normal/hover, visible/occluded 25% overlay (desktop GPU only)\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
