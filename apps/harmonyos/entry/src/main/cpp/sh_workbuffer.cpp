#include "sh_workbuffer.h"
#include "sh_shader.h"
#include <stdexcept>
#include <string>
namespace splat {
namespace {
GLuint Shader(GLenum type,const std::string& source){
 GLuint shader=glCreateShader(type);const char* text=source.c_str();glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
 GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[4096]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);glDeleteShader(shader);throw std::runtime_error(std::string("SH workbuffer shader: ")+log);}return shader;
}
}
bool ShWorkbuffer::Prepare(const std::shared_ptr<const Scene>& scene,const View& view,int degree,GLuint positions,const ShTexture& coefficients){
 if(scene->paged||scene->shDegree==0||degree==0){scene_.reset();degree_=-1;return false;}
 const auto& m=view.matrix;
 const std::array<float,3> eye={-(m[0]*m[12]+m[1]*m[13]+m[2]*m[14]),-(m[4]*m[12]+m[5]*m[13]+m[6]*m[14]),-(m[8]*m[12]+m[9]*m[13]+m[10]*m[14])};
 if(scene_==scene&&degree_==degree&&eye_==eye)return true;
 if(program_&&compressed_!=bool(scene->sogHarmonics))Destroy();
 compressed_=bool(scene->sogHarmonics);
 if(!program_){
  const auto vs=Shader(GL_VERTEX_SHADER,"#version 300 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}");
  const auto fs=Shader(GL_FRAGMENT_SHADER,std::string("#version 300 es\n")+(compressed_?"#define SH_SINGLE_SOG\n":"")+std::string("precision highp float;precision highp int;uniform sampler2D positions;uniform highp usampler2D encodedCodes;uniform sampler2D codebooks;uniform mat4 cameraView;uniform uint count;uint splatIndex;layout(location=0) out uvec4 outputColor;\n")+ShShader+R"(
void main(){
 splatIndex=uint(gl_FragCoord.y)*4096u+uint(gl_FragCoord.x);
 if(splatIndex>=count){outputColor=uvec4(0);return;}
 uint a=splatIndex*4u;vec3 position=texelFetch(positions,ivec2(int(a%4096u),int(a/4096u)),0).xyz;
 outputColor=floatBitsToUint(vec4(directionalColor(position,cameraView,vec3(0)),1));
})");
  program_=glCreateProgram();glAttachShader(program_,vs);glAttachShader(program_,fs);glLinkProgram(program_);glDeleteShader(vs);glDeleteShader(fs);
  GLint ok=0;glGetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("SH workbuffer link failed");
  glGenVertexArrays(1,&vao_);glGenFramebuffers(1,&fbo_);
 }
 const int rows=int((scene->Count()+4095)/4096);
 if(rows_!=rows){
  if(texture_)glDeleteTextures(1,&texture_);glGenTextures(1,&texture_);glActiveTexture(GL_TEXTURE9);glBindTexture(GL_TEXTURE_2D,texture_);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32UI,4096,rows);rows_=rows;
 }
 glBindFramebuffer(GL_FRAMEBUFFER,fbo_);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture_,0);
 if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("SH workbuffer framebuffer incomplete");
 glViewport(0,0,4096,rows_);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glUseProgram(program_);glBindVertexArray(vao_);
 coefficients.Bind(program_,degree);glUniform1i(glGetUniformLocation(program_,"shPaged"),0);
 // Keep unlike sampler types on distinct texture units even for dead branches.
 glUniform1i(glGetUniformLocation(program_,"encodedCodes"),1);glUniform1i(glGetUniformLocation(program_,"codebooks"),2);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,positions);glUniform1i(glGetUniformLocation(program_,"positions"),0);
 glUniformMatrix4fv(glGetUniformLocation(program_,"cameraView"),1,GL_FALSE,m.data());glUniform1ui(glGetUniformLocation(program_,"count"),scene->Count());
 glDrawArrays(GL_TRIANGLES,0,3);glBindFramebuffer(GL_FRAMEBUFFER,0);glEnable(GL_BLEND);
 scene_=scene;degree_=degree;eye_=eye;++updates_;return true;
}
void ShWorkbuffer::Bind(GLuint program,bool enabled)const{
 glUniform1i(glGetUniformLocation(program,"shColorCached"),enabled);glUniform1i(glGetUniformLocation(program,"shColorCache"),9);
 glActiveTexture(GL_TEXTURE9);glBindTexture(GL_TEXTURE_2D,texture_);glActiveTexture(GL_TEXTURE0);
}
void ShWorkbuffer::Destroy(){
 if(program_)glDeleteProgram(program_);if(vao_)glDeleteVertexArrays(1,&vao_);if(fbo_)glDeleteFramebuffers(1,&fbo_);if(texture_)glDeleteTextures(1,&texture_);
 program_=vao_=fbo_=texture_=0;rows_=0;degree_=-1;scene_.reset();
}
}
