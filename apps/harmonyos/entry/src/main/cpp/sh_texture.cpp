#include "sh_texture.h"
#include <stdexcept>
#include <cstring>
namespace splat {
bool ShTexture::Prepare(std::shared_ptr<const Scene> scene,size_t budget){
 if(scene_!=scene){Destroy();scene_=std::move(scene);}
 if(scene_&&scene_->sogHarmonics)return PrepareCompressed(budget);
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
bool ShTexture::PrepareCompressed(size_t budget){
 const auto &sh=*scene_->sogHarmonics;
 if(scene_->shLabels.size()!=scene_->Count()||scene_->paged)throw std::runtime_error("SOG SH source layout unsupported");
 if(!compressed_[0]){
  if(budget<sizeof(sh.books))return false;
  rows_=(scene_->Count()+4095)/4096;GLint limit=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
  if(rows_>size_t(limit))throw std::runtime_error("SOG SH texture exceeds GPU dimensions");
  glGenTextures(3,compressed_.data());
  const GLenum formats[]={GL_RG32UI,GL_RGBA8UI,GL_RG32F};
  const int widths[]={4096,sh.width,256},heights[]={int(rows_),sh.height,1};
  for(int i=0;i<3;i++){glActiveTexture(GL_TEXTURE6+i);glBindTexture(GL_TEXTURE_2D,compressed_[i]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexStorage2D(GL_TEXTURE_2D,1,formats[i],widths[i],heights[i]);}
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,1,GL_RG,GL_FLOAT,sh.books.data());
  budget=budget>sizeof(sh.books)?budget-sizeof(sh.books):0;
  compressedBytes_=rows_*4096*8+size_t(sh.width)*sh.height*4+sizeof(sh.books);
 }
 const size_t stride=size_t(sh.width)*4;
 const size_t centroids=std::min(size_t(sh.height)-centroidRow_,budget/stride);
 if(centroids){glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,compressed_[1]);glTexSubImage2D(GL_TEXTURE_2D,0,0,centroidRow_,sh.width,centroids,GL_RGBA_INTEGER,GL_UNSIGNED_BYTE,sh.centroids.data()+centroidRow_*stride);centroidRow_+=centroids;budget-=centroids*stride;}
 const size_t upload=std::min(rows_-row_,budget/(4096*8));
 if(upload){
  glActiveTexture(GL_TEXTURE6);glBindTexture(GL_TEXTURE_2D,compressed_[0]);
  const size_t complete=std::min(upload,(scene_->Count()-row_*4096)/4096);
  if(complete){glTexSubImage2D(GL_TEXTURE_2D,0,0,row_,4096,complete,GL_RG_INTEGER,GL_UNSIGNED_INT,scene_->shLabels.data()+row_*4096);row_+=complete;}
  if(complete<upload){std::array<std::array<uint32_t,2>,4096> tail{};std::copy(scene_->shLabels.begin()+row_*4096,scene_->shLabels.end(),tail.begin());glTexSubImage2D(GL_TEXTURE_2D,0,0,row_,4096,1,GL_RG_INTEGER,GL_UNSIGNED_INT,tail.data());row_++;}
 }
 glActiveTexture(GL_TEXTURE0);
 if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("SOG SH GPU upload failed");
 return row_==rows_&&centroidRow_==size_t(sh.height);
}
void ShTexture::Bind(GLuint program,int degree)const{
 const bool ready=(texture_||compressed_[0])&&row_==rows_&&(!scene_->sogHarmonics||centroidRow_==size_t(scene_->sogHarmonics->height));
 glUniform1i(glGetUniformLocation(program,"shCompressed"),ready&&compressed_[0]);
 for(int i=0;i<3;i++){glActiveTexture(GL_TEXTURE6+i);glBindTexture(GL_TEXTURE_2D,compressed_[i]);}
 glUniform1i(glGetUniformLocation(program,"shLabels"),6);glUniform1i(glGetUniformLocation(program,"shCentroids"),7);glUniform1i(glGetUniformLocation(program,"shBooks"),8);
 glUniform1i(glGetUniformLocation(program,"shSourceBands"),ready?scene_->shDegree:0);
 glUniform1i(glGetUniformLocation(program,"shBands"),ready?std::min(degree,scene_->shDegree):0);
 glUniform1i(glGetUniformLocation(program,"shFlip"),ready&&scene_->shTransform);
 glActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,texture_);glUniform1i(glGetUniformLocation(program,"shData"),5);glActiveTexture(GL_TEXTURE0);
}
void ShTexture::Destroy(){if(texture_)glDeleteTextures(1,&texture_);texture_=0;row_=rows_=0;scene_.reset();glDeleteTextures(3,compressed_.data());compressed_.fill(0);centroidRow_=compressedBytes_=0;}
}
