#include "sh_texture.h"
#include <stdexcept>
#include <cstring>
namespace splat {
bool ShTexture::Prepare(std::shared_ptr<const Scene> scene,size_t budget){
 if(scene_!=scene){Destroy();scene_=std::move(scene);}
 if(!scene_||scene_->harmonics.empty())return true;
 if(scene_->harmonics.size()!=scene_->Count()||scene_->paged)throw std::runtime_error("SH source layout unsupported");
 glActiveTexture(GL_TEXTURE5);
 if(!texture_){
  rows_=(scene_->harmonics.size()*48+16383)/16384;GLint limit=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
  if(rows_>size_t(limit))throw std::runtime_error("SH texture exceeds GPU dimensions");
  glGenTextures(1,&texture_);glBindTexture(GL_TEXTURE_2D,texture_);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,4096,rows_);
 }
 glBindTexture(GL_TEXTURE_2D,texture_);
 const size_t rows=std::min(rows_-row_,budget/(16384*sizeof(float)));
 if(rows){
  const auto *data=reinterpret_cast<const unsigned char *>(scene_->harmonics.data());const size_t values=scene_->harmonics.size()*48;
  const size_t complete=std::min(rows,(values-row_*16384)/16384);
  if(complete){glTexSubImage2D(GL_TEXTURE_2D,0,0,row_,4096,complete,GL_RGBA,GL_FLOAT,data+row_*16384*sizeof(float));row_+=complete;}
  if(complete<rows){std::array<float,16384> tail{};std::memcpy(tail.data(),data+row_*16384*sizeof(float),(values-row_*16384)*sizeof(float));glTexSubImage2D(GL_TEXTURE_2D,0,0,row_,4096,1,GL_RGBA,GL_FLOAT,tail.data());row_++;}
 }
 glActiveTexture(GL_TEXTURE0);
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("SH GPU upload failed");
 return row_==rows_;
}
void ShTexture::Bind(GLuint program,int degree)const{
 const bool ready=texture_&&row_==rows_;
 glUniform1i(glGetUniformLocation(program,"shBands"),ready?std::min(degree,scene_->shDegree):0);
 glUniform1i(glGetUniformLocation(program,"shFlip"),ready&&scene_->shTransform);
 glActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,texture_);glUniform1i(glGetUniformLocation(program,"shData"),5);glActiveTexture(GL_TEXTURE0);
}
void ShTexture::Destroy(){if(texture_)glDeleteTextures(1,&texture_);texture_=0;row_=rows_=0;scene_.reset();}
}
