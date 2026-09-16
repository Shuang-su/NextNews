#pragma once
#include "splat.h"
#include <GLES3/gl3.h>
namespace splat {
class ShTexture {
public:
 ShTexture()=default;
 ShTexture(const ShTexture&)=delete;
 ShTexture& operator=(const ShTexture&)=delete;
 void Swap(ShTexture& other){scene_.swap(other.scene_);std::swap(texture_,other.texture_);std::swap(row_,other.row_);std::swap(rows_,other.rows_);compressed_.swap(other.compressed_);std::swap(centroidRow_,other.centroidRow_);std::swap(compressedBytes_,other.compressedBytes_);}
 bool Prepare(std::shared_ptr<const Scene> scene,size_t budget);
 void Bind(GLuint program,int degree)const;
 void Destroy();
 size_t Bytes()const{return compressedBytes_?compressedBytes_:size_t(rows_)*4096*16;}
private:
 std::shared_ptr<const Scene> scene_;
 GLuint texture_=0;
 size_t row_=0,rows_=0;
 std::array<GLuint,3> compressed_{};
 size_t centroidRow_=0,compressedBytes_=0;
 bool PrepareCompressed(size_t budget);
};
}
