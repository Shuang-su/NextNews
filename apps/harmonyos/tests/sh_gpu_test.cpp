// Desktop GPU check of the production SH shader. Not phone rendering evidence.
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include "sh_shader.h"
#include "sh_pages.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
GLuint shader(GLenum kind,const std::string& source){
 GLuint s=glCreateShader(kind);const char* p=source.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);
 GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(s,sizeof(log),nullptr,log);throw std::runtime_error(log);}return s;
}
GLuint texture(int unit,GLenum internal,int w,int h,GLenum format,GLenum type,const void* data){
 GLuint t;glGenTextures(1,&t);glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,t);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexImage2D(GL_TEXTURE_2D,0,internal,w,h,0,format,type,data);return t;
}
// Independent real spherical harmonic basis using associated Legendre recurrence,
// rather than copying PlayCanvas's Cartesian polynomial implementation.
double basis(int l,int m,const std::array<double,3>& d){
 const int a=std::abs(m);double p=1;
 for(int k=1;k<=a;k++)p*=-(2*k-1)*std::sqrt(std::max(0.,1-d[2]*d[2]));
 if(l>a){double previous=p;p=d[2]*(2*a+1)*p;
  for(int k=a+2;k<=l;k++){double next=((2*k-1)*d[2]*p-(k+a-1)*previous)/(k-a);previous=p;p=next;}}
 double ratio=1;for(int k=l-a+1;k<=l+a;k++)ratio/=k;
 double norm=std::sqrt((2*l+1)/(4*std::acos(-1.))*ratio);
 if(m==0)return norm*p;
 const double angle=a*std::atan2(d[1],d[0]);return std::sqrt(2.)*norm*p*(m<0?std::sin(angle):std::cos(angle));
}
uint8_t code(int label,int coefficient,int channel){return uint8_t(1+(label*17+coefficient*13+channel*37)%254);}
float book(int value,int source){return float(value-127)/127.f*(source==1?.12f:-.09f);}
int main(){try{
 CGLPixelFormatAttribute attrs[]={kCGLPFAOpenGLProfile,(CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,kCGLPFAAccelerated,(CGLPixelFormatAttribute)0};
 CGLPixelFormatObj pf=nullptr;GLint count=0;CGLContextObj ctx=nullptr;
 if(CGLChoosePixelFormat(attrs,&pf,&count)||!pf||CGLCreateContext(pf,nullptr,&ctx))throw std::runtime_error("Desktop OpenGL context unavailable");
 CGLDestroyPixelFormat(pf);CGLSetCurrentContext(ctx);
 const std::string fragment=std::string("#version 410 core\nuniform uint splatIndex;uniform usampler2D encodedCodes;uniform sampler2D codebooks;uniform vec3 direction;out vec4 result;\n")+splat::ShShader+"\nvoid main(){result=vec4(shBands>0?directionalColor(direction,mat4(1.0),vec3(.5)):vec3(.5),1.0);}";
 GLuint vs=shader(GL_VERTEX_SHADER,"#version 410 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}");
 GLuint fs=shader(GL_FRAGMENT_SHADER,fragment),program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
 GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Shader link failed");glUseProgram(program);
 auto uni=[&](const char* name,int value){glUniform1i(glGetUniformLocation(program,name),value);};
 uni("encodedCodes",1);uni("codebooks",2);uni("shData",3);uni("shLabels",6);uni("shCentroids",7);uni("shBooks",8);uni("shDecoded",10);
 GLuint vao,fbo;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
 GLuint output=texture(0,GL_RGBA32F,1,1,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,output,0);
 if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Test framebuffer incomplete");glViewport(0,0,1,1);
 std::vector<float> books(256*3*4),singleBooks(256*2);
 for(int s=1;s<=2;s++)for(int i=0;i<256;i++){books[(s*256+i)*4+2]=book(i,s);books[(s*256+i)*4+3]=.5f;}
 GLuint tb=texture(2,GL_RGBA32F,256,3,GL_RGBA,GL_FLOAT,books.data());
 std::vector<uint32_t> mapping(64*3);for(int s=1;s<=2;s++)for(int p=0;p<64;p++)mapping[s*64+p]=(p+7)%64;
 size_t trials=0;double worst=0;
 for(int degree=1;degree<=3;degree++){
  const int n=(degree+1)*(degree+1)-1;const std::array<int,5> labels={0,63,64,1092,65535};
  std::vector<uint8_t> centroids(64*n*1024*4),paged(4096*256*4);
  for(int label:labels)for(int k=0;k<n;k++)for(int c=0;c<3;c++){
   uint32_t a=label*n+k,p=mapping[64+a/16384]*16384+a%16384;
   centroids[a*4+c]=paged[p*4+c]=code(label,k,c);
  }
  for(int source=1;source<=2;source++){
   for(int i=0;i<256;i++){singleBooks[i*2]=0;singleBooks[i*2+1]=book(i,source);}
   GLuint sb=texture(8,GL_RG32F,256,1,GL_RG,GL_FLOAT,singleBooks.data());
   for(int mode=0;mode<3;mode++){
    uni("shPaged",mode==2);uni("shCompressed",mode==1);uni("shSourceBands",degree);
    std::vector<float> decoded(centroids.size());
    for(size_t i=0;i<decoded.size();i++)decoded[i]=book(centroids[i],source);
    GLuint ct=mode==2?texture(7,GL_RGBA8UI,4096,256,GL_RGBA_INTEGER,GL_UNSIGNED_BYTE,paged.data()):
      texture(10,GL_RGBA32F,64*n,1024,GL_RGBA,GL_FLOAT,decoded.data());
    for(int label:labels){
     // Draw index is unrelated to centroid identity, modelling reordered addresses.
     const uint32_t index=(label*31+79)%4096;glUniform1ui(glGetUniformLocation(program,"splatIndex"),index);
     std::vector<uint32_t> references(4096*4),singleLabels(4096*2);
     references[index*4+3]=splat::SogReference(source,label,degree);singleLabels[index*2]=label;
     GLuint refs=texture(1,GL_RGBA32UI,4096,1,GL_RGBA_INTEGER,GL_UNSIGNED_INT,references.data());
     GLuint maps=mode==2?texture(6,GL_R32UI,64,3,GL_RED_INTEGER,GL_UNSIGNED_INT,mapping.data()):texture(6,GL_RG32UI,4096,1,GL_RG_INTEGER,GL_UNSIGNED_INT,singleLabels.data());
     std::vector<float> values(4096*12*4);for(int c=0;c<3;c++)values[index*48+c]=.5f;
     for(int k=0;k<n;k++)for(int c=0;c<3;c++)values[index*48+3+k*3+c]=book(code(label,k,c),source);
     GLuint data=texture(3,GL_RGBA32F,4096,12,GL_RGBA,GL_FLOAT,values.data());
     for(int band=0;band<=3;band++)for(int flip=0;flip<=1;flip++)for(auto dir:std::vector<std::array<double,3>>{{0,0,1},{1,0,0},{.3,.4,-.5},{-.7,.2,.1}}){
      double length=std::sqrt(dir[0]*dir[0]+dir[1]*dir[1]+dir[2]*dir[2]);for(auto& v:dir)v/=length;
      glUniform3f(glGetUniformLocation(program,"direction"),dir[0],dir[1],dir[2]);uni("shBands",band);uni("shFlip",flip);
      if(flip){dir[0]*=-1;dir[1]*=-1;}
      std::array<double,3> expected={.5,.5,.5};int k=0;
      for(int l=1;l<=std::min(band,degree);l++)for(int m=-l;m<=l;m++,k++)for(int c=0;c<3;c++)expected[c]+=basis(l,m,dir)*book(code(label,k,c),source);
      glDrawArrays(GL_TRIANGLES,0,3);float actual[4];glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,actual);
      for(int c=0;c<3;c++){double error=std::abs(actual[c]-std::max(0.,expected[c]));worst=std::max(worst,error);if(!std::isfinite(actual[c])||error>2e-5)throw std::runtime_error("SH GPU/reference mismatch mode="+std::to_string(mode)+" degree="+std::to_string(degree)+" label="+std::to_string(label)+" error="+std::to_string(error));}trials++;
     }
     if(mode==2){
      // An SH0 source in a mixed-degree selection must use its base color.
      uint32_t dcReference[4]={0,0,0,splat::SogReference(source,0,0)};
      glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,refs);
      glTexSubImage2D(GL_TEXTURE_2D,0,index,0,1,1,GL_RGBA_INTEGER,GL_UNSIGNED_INT,dcReference);
      uni("shBands",3);glDrawArrays(GL_TRIANGLES,0,3);float actual[4];glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,actual);
      for(int c=0;c<3;c++)if(actual[c]!=.5f)throw std::runtime_error("Mixed SH0 source sampled higher coefficients");trials++;
     }
     glDeleteTextures(1,&refs);glDeleteTextures(1,&maps);glDeleteTextures(1,&data);
    }
    glDeleteTextures(1,&ct);
   }
   glDeleteTextures(1,&sb);
  }
 }
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("OpenGL error during test");
 std::cout<<"PASS "<<trials<<" desktop GPU samples, float/compressed/paged SH1-3; worst error="<<worst<<"; renderer="<<glGetString(GL_RENDERER)<<"\n";
 glDeleteTextures(1,&tb);glDeleteTextures(1,&output);glDeleteProgram(program);glDeleteShader(vs);glDeleteShader(fs);glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);CGLSetCurrentContext(nullptr);CGLDestroyContext(ctx);
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
