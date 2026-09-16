#pragma once
#include "collision.h"
namespace viewer {
// Static GLB collision geometry, in the same baked world coordinates as the
// upstream fromGlb loader. No model/viewer rotation is applied a second time.
class Mesh final:public CollisionResource {
public:
    struct Triangle {std::array<float,3> a,b,c,n;};
    static std::shared_ptr<Mesh> Load(const std::string &path);
    Mesh(const std::vector<V3> &positions,const std::vector<uint32_t> &indices);
    std::optional<V3> Ray(V3 origin,V3 direction,double maxDistance)const override;
    bool Deepest(V3 center,double halfHeight,double radius,V3 &push)const override;
    bool Free(V3 point)const override;
    bool Known(Box)const override{return !triangles_.empty();}
    double Resolution()const override{return .05;}
    size_t Bytes()const override;
    Box Bounds()const override{return nodes_.front().bounds;}
    bool Available()const override{return !triangles_.empty();}
    bool CompleteWorld()const override{return Available();}
    void Debug(Box area,DebugWire &wire)const override;
private:
    struct Node {Box bounds;uint32_t start=0,count=0,left=0,right=0;};
    uint32_t Build(uint32_t start,uint32_t count,int depth);
    std::vector<Triangle> triangles_;
    std::vector<uint32_t> order_;
    std::vector<Node> nodes_;
};
}
