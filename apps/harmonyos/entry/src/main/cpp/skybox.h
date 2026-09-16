#pragma once
#include "sky_image.h"
#include "splat.h"
#include <GLES3/gl3.h>
namespace splat {
class Skybox {
public:
 void Prepare(std::shared_ptr<const SkyImage> image);
 void Draw(const View& view,int width,int height,int tone);
 void Destroy();
 bool Pending()const{return image_&&row_<image_->height;}
 size_t Bytes()const{return image_?image_->Bytes()+size_t(image_->width)*image_->height*8:0;}
private:
 std::shared_ptr<const SkyImage> image_;
 GLuint texture_=0,program_=0,vao_=0;
 int row_=0;
};
}
