#pragma once
#include "splat.h"
#include <GLES3/gl3.h>
#include <memory>
namespace splat {
struct HotspotData {std::vector<float> positions;std::vector<uint8_t> glyphs;};
struct HotspotStyle {bool visible=false;int hover=-1;float pixels=25;};
// Same composition as SuperSplat v1.31.2: depth-writing base before splats,
// then a 25% overlay. No depth readback or per-annotation scene traversal.
class Hotspots {
public:
    bool Prepare(const std::shared_ptr<const HotspotData>& data);
    void Draw(const View&,int width,int height,HotspotStyle style,bool overlay,int directTone=0);
    void Destroy();
    bool Failed()const{return failed_;}
private:
    GLuint program_=0,texture_=0,vao_=0,buffer_=0;
    std::shared_ptr<const HotspotData> uploaded_;
    bool failed_=false;
};
}
