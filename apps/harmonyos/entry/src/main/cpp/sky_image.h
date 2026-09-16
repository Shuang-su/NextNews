#pragma once
#include <memory>
#include <string>
#include <cstddef>
namespace splat {
struct SkyImage {
 int width=0,height=0;
 std::shared_ptr<float> pixels;
 size_t Bytes()const{return size_t(width)*height*4*sizeof(float);}
};
std::shared_ptr<SkyImage> ReadSkyImage(const std::string& path);
}
