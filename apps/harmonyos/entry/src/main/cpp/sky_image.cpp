#include "sky_image.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_HDR
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include "third_party/stb/stb_image.h"
#include <fstream>
#include <vector>
#include <cmath>
#include <stdexcept>
namespace splat {
std::shared_ptr<SkyImage> ReadSkyImage(const std::string& path){
 std::ifstream file(path,std::ios::binary|std::ios::ate);
 if(!file)throw std::runtime_error("Cannot open skybox image");
 const auto bytes=file.tellg();if(bytes<=0||bytes>64*1024*1024)throw std::runtime_error("Skybox file exceeds 64 MiB");
 std::vector<unsigned char> input(static_cast<size_t>(bytes));file.seekg(0);if(!file.read(reinterpret_cast<char*>(input.data()),bytes))throw std::runtime_error("Truncated skybox image");
 int w=0,h=0,channels=0;
 if(!stbi_info_from_memory(input.data(),int(input.size()),&w,&h,&channels))throw std::runtime_error("Expected HDR, PNG or JPEG skybox");
 if(w<2||h<1||w!=2*h||size_t(w)*h>8*1024*1024)throw std::runtime_error("Skybox must be a 2:1 panorama within 8 megapixels");
 auto image=std::make_shared<SkyImage>();image->width=w;image->height=h;
 if(stbi_is_hdr_from_memory(input.data(),int(input.size()))) {
  image->pixels=std::shared_ptr<float>(stbi_loadf_from_memory(input.data(),int(input.size()),&w,&h,&channels,4),stbi_image_free);
 } else {
  // MetaFlow loadSkybox forces RGBP: retain bytes, including alpha, before decoding.
  std::unique_ptr<unsigned char,decltype(&stbi_image_free)> raw(stbi_load_from_memory(input.data(),int(input.size()),&w,&h,&channels,4),stbi_image_free);
  if(!raw)throw std::runtime_error("Skybox decode failed");
  image->pixels=std::shared_ptr<float>(new float[size_t(w)*h*4],std::default_delete<float[]>());
  for(size_t i=0;i<size_t(w)*h;i++) {
   const float multiplier=8-7*(raw.get()[i*4+3]/255.f);
   for(int c=0;c<3;c++){const float value=raw.get()[i*4+c]/255.f*multiplier;image->pixels.get()[i*4+c]=value*value;}
   image->pixels.get()[i*4+3]=1;
  }
 }
 if(!image->pixels)throw std::runtime_error("Skybox decode failed");
 if(w!=image->width||h!=image->height)throw std::runtime_error("Skybox dimensions changed during decode");
 for(size_t i=0;i<image->Bytes()/sizeof(float);i++)if(!std::isfinite(image->pixels.get()[i])||image->pixels.get()[i]<0||image->pixels.get()[i]>65504)throw std::runtime_error("Skybox exceeds finite RGBA16F range");
 return image;
}
}
