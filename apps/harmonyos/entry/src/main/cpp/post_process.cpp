#include "post_process.h"
#include "post_shaders.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
namespace splat {
namespace {
GLuint Shader(GLenum type,const char* source){
 GLuint id=glCreateShader(type);glShaderSource(id,1,&source,nullptr);glCompileShader(id);GLint ok=0;glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[4096]{};glGetShaderInfoLog(id,sizeof(log),nullptr,log);glDeleteShader(id);throw std::runtime_error(std::string("Post shader: ")+log);}return id;
}
GLuint Program(const char* fragment){
 GLuint v=Shader(GL_VERTEX_SHADER,PostVertex),f=0;try{f=Shader(GL_FRAGMENT_SHADER,fragment);}catch(...){glDeleteShader(v);throw;}
 GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
 if(!ok){char log[4096]{};glGetProgramInfoLog(p,sizeof(log),nullptr,log);glDeleteProgram(p);throw std::runtime_error(std::string("Post link: ")+log);}return p;
}
}
void PostProcess::Release(Target& t){if(t.framebuffer)glDeleteFramebuffers(1,&t.framebuffer);if(t.texture)glDeleteTextures(1,&t.texture);t={};}
PostProcess::Target PostProcess::Create(int width,int height,bool high){
 Target t;t.width=width;t.height=height;glGenTextures(1,&t.texture);glBindTexture(GL_TEXTURE_2D,t.texture);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glTexImage2D(GL_TEXTURE_2D,0,high?GL_RGBA16F:GL_RGBA8,width,height,0,GL_RGBA,high?GL_HALF_FLOAT:GL_UNSIGNED_BYTE,nullptr);
 glGenFramebuffers(1,&t.framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,t.framebuffer);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t.texture,0);
 if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){Release(t);throw std::runtime_error("Requested post-process framebuffer is unsupported");}return t;
}
void PostProcess::Storage(){
 Release(scene_);for(auto& t:bloom_)Release(t);bloom_.clear();if(depth_)glDeleteRenderbuffers(1,&depth_);depth_=0;
}
bool PostProcess::Begin(int width,int height,const Effects& settings){
 settings_=settings;
 if(settings[21]==0){if(scene_.texture)Storage();glBindFramebuffer(GL_FRAMEBUFFER,0);return false;}
 if(!compose_){compose_=Program(PostCompose);down_=Program(PostDown);up_=Program(PostUp);glGenVertexArrays(1,&vao_);}
 const bool high=settings[1]>0;
 if(scene_.width!=width||scene_.height!=height||high_!=high){
  Storage();high_=high;scene_=Create(width,height,high);
  glGenRenderbuffers(1,&depth_);glBindRenderbuffer(GL_RENDERBUFFER,depth_);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
  glBindFramebuffer(GL_FRAMEBUFFER,scene_.framebuffer);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth_);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Post-process depth buffer is unsupported");
 }
 const int levels=settings[4]>0&&high?std::clamp(int(settings[6]),1,std::max(1,int(std::floor(std::log2(std::min(width,height)))))):0;
 if(int(bloom_.size())!=levels){for(auto& t:bloom_)Release(t);bloom_.clear();int w=width,h=height;for(int i=0;i<levels;i++){w=std::max(1,w/2);h=std::max(1,h/2);bloom_.push_back(Create(w,h,high));}}
 glBindFramebuffer(GL_FRAMEBUFFER,scene_.framebuffer);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);return true;
}
void PostProcess::Pass(GLuint program,const Target& source,const Target& target,bool additive){
 glBindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);glViewport(0,0,target.width,target.height);glUseProgram(program);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,source.texture);glUniform1i(glGetUniformLocation(program,"sourceTexture"),0);
 glUniform2f(glGetUniformLocation(program,"sourceInvResolution"),1.f/source.width,1.f/source.height);
 if(additive){glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);}else glDisable(GL_BLEND);
 glDrawArrays(GL_TRIANGLES,0,3);
}
void PostProcess::Finish(){
 glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glBindVertexArray(vao_);
 const Target* source=&scene_;for(auto& target:bloom_){Pass(down_,*source,target);source=&target;}
 for(int i=int(bloom_.size())-2;i>=0;i--)Pass(up_,bloom_[i+1],bloom_[i],true);
 glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,scene_.width,scene_.height);glDisable(GL_BLEND);glUseProgram(compose_);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,scene_.texture);glUniform1i(glGetUniformLocation(compose_,"sceneTexture"),0);
 glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,bloom_.empty()?scene_.texture:bloom_[0].texture);glUniform1i(glGetUniformLocation(compose_,"bloomTexture"),1);
 glUniform2f(glGetUniformLocation(compose_,"sceneTextureInvRes"),1.f/scene_.width,1.f/scene_.height);
 glUniform1fv(glGetUniformLocation(compose_,"settings"),settings_.size(),settings_.data());
 glUniform1f(glGetUniformLocation(compose_,"sharpness"),-.125f-.075f*settings_[2]);
 glUniform1f(glGetUniformLocation(compose_,"fringingIntensity"),settings_[3]/1024.f);
 glUniform3f(glGetUniformLocation(compose_,"brightnessContrastSaturation"),settings_[8],settings_[9],settings_[10]);
 glUniform3f(glGetUniformLocation(compose_,"tint"),settings_[11],settings_[12],settings_[13]);
 glUniform4f(glGetUniformLocation(compose_,"vignetterParams"),settings_[15],settings_[16],settings_[17],settings_[18]);
 glUniform3f(glGetUniformLocation(compose_,"vignetteColor"),0,0,0);glDrawArrays(GL_TRIANGLES,0,3);
 glActiveTexture(GL_TEXTURE0);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
}
void PostProcess::Destroy(){Storage();for(GLuint p:{compose_,down_,up_})if(p)glDeleteProgram(p);compose_=down_=up_=0;if(vao_)glDeleteVertexArrays(1,&vao_);vao_=0;}
}
