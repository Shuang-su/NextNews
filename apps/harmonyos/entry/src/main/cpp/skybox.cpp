#include "skybox.h"
#include "post_shaders.h"
#include <stdexcept>
#include <string>
#include <algorithm>
namespace splat {
namespace {
const char* Fragment=R"SKY(#version 300 es
precision highp float;
/*TONE*/
in vec2 uv0;
out vec4 outColor;
uniform sampler2D panorama;
uniform mat4 view;
uniform vec2 lens;
uniform int tone;
void main(){
 vec2 screen=uv0*2.0-1.0;
 vec3 dir=normalize(transpose(mat3(view))*vec3(screen.x*lens.x*lens.y,screen.y*lens.x,-1.0));
 dir.x=-dir.x;
 const float PI=3.141592653589793;
 vec2 uv=vec2((dir.x==0.0&&dir.z==0.0)?0.0:atan(dir.x,dir.z),asin(clamp(dir.y,-1.0,1.0)))/vec2(2.0*PI,PI)+0.5;uv.y=1.0-uv.y;
 vec3 color=texture(panorama,uv).rgb;
 if(tone==2)color=toneMap2(color);else if(tone==3)color=toneMap3(color);else if(tone==4)color=toneMap4(color);else if(tone==5)color=toneMap5(color);else if(tone==6)color=toneMap6(color);
 outColor=vec4(pow(max(color,vec3(0))+0.0000001,vec3(1.0/2.2)),1);
})SKY";
GLuint Compile(GLenum type,const char* text){GLuint s=glCreateShader(type);glShaderSource(s,1,&text,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[2048]{};glGetShaderInfoLog(s,sizeof(log),nullptr,log);glDeleteShader(s);throw std::runtime_error(std::string("Skybox shader: ")+log);}return s;}
}
void Skybox::Prepare(std::shared_ptr<const SkyImage> image){
 if(image_!=image){if(texture_)glDeleteTextures(1,&texture_);texture_=0;row_=0;image_=std::move(image);}
 if(!image_)return;
 if(!program_){
  std::string fragment=Fragment;fragment.replace(fragment.find("/*TONE*/"),8,PostTone);
  GLuint v=Compile(GL_VERTEX_SHADER,PostVertex),f=0;try{f=Compile(GL_FRAGMENT_SHADER,fragment.c_str());}catch(...){glDeleteShader(v);throw;}
  program_=glCreateProgram();glAttachShader(program_,v);glAttachShader(program_,f);glLinkProgram(program_);glDeleteShader(v);glDeleteShader(f);
  GLint ok=0;glGetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Skybox program link failed");glGenVertexArrays(1,&vao_);
 }
 glActiveTexture(GL_TEXTURE4);
 if(!texture_){glGenTextures(1,&texture_);glBindTexture(GL_TEXTURE_2D,texture_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,image_->width,image_->height,0,GL_RGBA,GL_FLOAT,nullptr);}
 if(row_<image_->height){glBindTexture(GL_TEXTURE_2D,texture_);const int rows=std::min(image_->height-row_,std::max(1,int(4*1024*1024/(size_t(image_->width)*16))));glTexSubImage2D(GL_TEXTURE_2D,0,0,row_,image_->width,rows,GL_RGBA,GL_FLOAT,image_->pixels.get()+size_t(row_)*image_->width*4);row_+=rows;}
 glActiveTexture(GL_TEXTURE0);if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Skybox texture upload unsupported");
}
void Skybox::Draw(const View& view,int width,int height,int tone){
 if(!image_||Pending()||!texture_||!program_)return;
 glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glUseProgram(program_);glBindVertexArray(vao_);
 glUniformMatrix4fv(glGetUniformLocation(program_,"view"),1,GL_FALSE,view.matrix.data());glUniform2f(glGetUniformLocation(program_,"lens"),view.tanHalfFov,float(width)/height);glUniform1i(glGetUniformLocation(program_,"tone"),tone);
 glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,texture_);glUniform1i(glGetUniformLocation(program_,"panorama"),4);glDrawArrays(GL_TRIANGLES,0,3);glActiveTexture(GL_TEXTURE0);
 glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
}
void Skybox::Destroy(){if(texture_)glDeleteTextures(1,&texture_);if(program_)glDeleteProgram(program_);if(vao_)glDeleteVertexArrays(1,&vao_);texture_=program_=vao_=0;image_.reset();row_=0;}
}
