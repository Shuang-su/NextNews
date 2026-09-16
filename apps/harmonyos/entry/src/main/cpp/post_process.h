#pragma once
#include <GLES3/gl3.h>
#include <array>
#include <cstddef>
#include <vector>
namespace splat {
// Immutable frame snapshot: tone, high precision, sharpness, fringe, bloom enabled/intensity/levels,
// grade enabled/brightness/contrast/saturation/tint.rgb, vignette enabled/inner/outer/curve/intensity,
// reserved, reserved, master enabled.
using Effects=std::array<float,22>;
class PostProcess {
public:
 bool Begin(int width,int height,const Effects& settings);
 void Finish();
 void Destroy();
 size_t Bytes() const {size_t n=size_t(scene_.width)*scene_.height*(high_?12:8);for(const auto& t:bloom_)n+=size_t(t.width)*t.height*(high_?8:4);return n;}
private:
 struct Target {GLuint framebuffer=0,texture=0;int width=0,height=0;};
 Target scene_;std::vector<Target> bloom_;
 GLuint depth_=0,vao_=0,compose_=0,down_=0,up_=0;
 Effects settings_{};bool high_=false;
 static void Release(Target& target);
 static Target Create(int width,int height,bool high);
 void Pass(GLuint program,const Target& source,const Target& target,bool additive=false);
 void Storage();
};
}
