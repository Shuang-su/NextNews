// Adapted from PlayCanvas SuperSplat Viewer v1.31.2, MIT. See THIRD_PARTY_NOTICES.
#include "mesh.h"
#include "../third_party/nlohmann/json.hpp"
#include <fstream>
#include <limits>
#include <numeric>
#include <cstring>
#include <stdexcept>
namespace viewer {
namespace {
using Json=nlohmann::json;
constexpr size_t MaxBytes=64*1024*1024,MaxTriangles=1000000;
void Check(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
V3 V(const std::array<float,3> &v){return {v[0],v[1],v[2]};}
std::array<float,3> F(V3 v){return {float(v.x),float(v.y),float(v.z)};}
bool Valid(V3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<=1e6&&std::abs(v.y)<=1e6&&std::abs(v.z)<=1e6;}
V3 Cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Box Empty(){double inf=std::numeric_limits<double>::infinity();return {{inf,inf,inf},{-inf,-inf,-inf}};}
void Expand(Box &b,V3 p){for(int k=0;k<3;k++){b.min[k]=std::min(b.min[k],p[k]);b.max[k]=std::max(b.max[k],p[k]);}}
bool Overlap(Box a,Box b){for(int k=0;k<3;k++)if(a.max[k]<b.min[k]||a.min[k]>b.max[k])return false;return true;}
bool RayBox(Box b,V3 o,V3 d,double limit){
    double lo=0,hi=limit;
    for(int k=0;k<3;k++){
        if(std::abs(d[k])<1e-12){if(o[k]<b.min[k]||o[k]>b.max[k])return false;continue;}
        double a=(b.min[k]-o[k])/d[k],c=(b.max[k]-o[k])/d[k];if(a>c)std::swap(a,c);lo=std::max(lo,a);hi=std::min(hi,c);if(lo>hi)return false;
    }return true;
}
double RayTriangle(V3 o,V3 d,V3 a,V3 b,V3 c){
    auto e1=b-a,e2=c-a,p=Cross(d,e2);double det=e1.Dot(p);if(std::abs(det)<1e-10)return -1;
    auto t=o-a;double u=t.Dot(p)/det;if(u<0||u>1)return -1;
    auto q=Cross(t,e1);double v=d.Dot(q)/det;if(v<0||u+v>1)return -1;
    return e2.Dot(q)/det;
}
V3 Closest(V3 p,V3 a,V3 b,V3 c){
    auto ab=b-a,ac=c-a,ap=p-a;double d1=ab.Dot(ap),d2=ac.Dot(ap);if(d1<=0&&d2<=0)return a;
    auto bp=p-b;double d3=ab.Dot(bp),d4=ac.Dot(bp);if(d3>=0&&d4<=d3)return b;
    double vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+ab*(d1/(d1-d3));
    auto cp=p-c;double d5=ab.Dot(cp),d6=ac.Dot(cp);if(d6>=0&&d5<=d6)return c;
    double vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+ac*(d2/(d2-d6));
    double va=d3*d6-d5*d4;if(va<=0&&d4-d3>=0&&d5-d6>=0)return b+(c-b)*((d4-d3)/(d4-d3+d5-d6));
    return a+ab*(vb/(va+vb+vc))+ac*(vc/(va+vb+vc));
}
V3 Segment(V3 p,V3 a,V3 b){auto d=b-a;double length=d.Dot(d);return length<1e-20?a:a+d*std::clamp((p-a).Dot(d)/length,0.0,1.0);}
// Match upstream's six samples plus one refinement for a vertical capsule.
void ClosestSegment(V3 s0,V3 s1,V3 a,V3 b,V3 c,V3 &sp,V3 &tp){
    double best=std::numeric_limits<double>::infinity();
    for(int i=0;i<=5;i++){auto p=s0+(s1-s0)*(i/5.0),q=Closest(p,a,b,c);double d=(p-q).Dot(p-q);if(d<best){best=d;sp=p;tp=q;}}
    auto p=Segment(tp,s0,s1),q=Closest(p,a,b,c);if((p-q).Dot(p-q)<best){sp=p;tp=q;}
}
uint32_t U32(const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
size_t Integer(const Json &v,size_t max=MaxBytes){Check(v.is_number_integer(),"GLB count/index must be an integer");auto n=v.get<int64_t>();Check(n>=0&&uint64_t(n)<=max,"GLB count/index out of range");return size_t(n);}
size_t Optional(const Json &j,const char *name,size_t fallback){return j.contains(name)?Integer(j.at(name)):fallback;}
struct Accessor {const uint8_t *data;size_t count,stride;int component;};
Accessor GetAccessor(const Json &j,const std::vector<uint8_t> &bin,size_t index,bool position){
    const auto &accessors=j.at("accessors");Check(accessors.is_array()&&index<accessors.size(),"GLB accessor missing");const auto &a=accessors.at(index);
    Check(!a.contains("sparse"),"Sparse GLB collision accessors are not supported");
    Check(a.value("normalized",false)==false,"Normalized GLB collision positions are not supported");
    Check(a.value("type",std::string())==(position?"VEC3":"SCALAR"),"Invalid GLB collision accessor type");
    const int component=int(Integer(a.at("componentType"),65535));
    Check(position?component==5126:(component==5121||component==5123||component==5125),"Unsupported GLB collision component");
    const size_t unit=component==5121?1:component==5123?2:4,element=unit*(position?3:1),count=Integer(a.at("count"),MaxTriangles*3);
    Check(count>0,"Empty GLB collision accessor");const auto &views=j.at("bufferViews");const auto vi=Integer(a.at("bufferView"));Check(views.is_array()&&vi<views.size(),"GLB bufferView missing");const auto &view=views.at(vi);
    Check(Integer(view.at("buffer"))==0&&!view.contains("extensions"),"Unsupported GLB collision buffer");
    const auto offset=Optional(view,"byteOffset",0),length=Integer(view.at("byteLength")),start=Optional(a,"byteOffset",0),stride=Optional(view,"byteStride",element);
    Check(stride>=element&&stride<=252&&stride%unit==0&&start%unit==0&&offset%unit==0,"Invalid GLB collision stride/alignment");
    Check(offset<=bin.size()&&length<=bin.size()-offset&&start<=length&&element<=length-start&&(count-1)<=(length-start-element)/stride,"GLB collision accessor exceeds buffer");
    return {bin.data()+offset+start,count,stride,component};
}
}
Mesh::Mesh(const std::vector<V3> &positions,const std::vector<uint32_t> &indices){
    Check(!indices.empty()&&indices.size()%3==0&&indices.size()/3<=MaxTriangles,"Invalid GLB triangle count");
    triangles_.reserve(indices.size()/3);
    for(size_t i=0;i<indices.size();i+=3){
        Check(indices[i]<positions.size()&&indices[i+1]<positions.size()&&indices[i+2]<positions.size(),"GLB triangle index out of range");
        auto a=positions[indices[i]],b=positions[indices[i+1]],c=positions[indices[i+2]];
        Check(Valid(a)&&Valid(b)&&Valid(c),"Nonfinite or excessive GLB collision position");
        auto normal=Cross(b-a,c-a);auto length=normal.Length();if(length<1e-10)continue;
        triangles_.push_back({F(a),F(b),F(c),F(normal*(1/length))});
    }
    Check(!triangles_.empty(),"GLB contains no nondegenerate collision triangles");
    order_.resize(triangles_.size());std::iota(order_.begin(),order_.end(),0);nodes_.reserve(triangles_.size());Build(0,uint32_t(triangles_.size()),0);
}
uint32_t Mesh::Build(uint32_t start,uint32_t count,int depth){
    Box b=Empty();for(uint32_t i=start;i<start+count;i++){const auto &t=triangles_[order_[i]];Expand(b,V(t.a));Expand(b,V(t.b));Expand(b,V(t.c));}
    uint32_t id=uint32_t(nodes_.size());nodes_.push_back({b,start,count,0,0});if(count<=4)return id;
    V3 size=b.max-b.min;int axis=size.x>=size.y&&size.x>=size.z?0:size.y>=size.z?1:2;double mid=(b.min[axis]+b.max[axis])*.5;
    uint32_t split=start;
    // Bound BVH recursion for adversarial midpoint distributions.
    if(depth<48){
        uint32_t right=start+count;
        while(split<right){const auto &t=triangles_[order_[split]];if((double(t.a[axis])+t.b[axis]+t.c[axis])/3.0<mid)split++;else std::swap(order_[split],order_[--right]);}
    }
    if(split==start||split==start+count)split=start+count/2;
    const auto left=Build(start,split-start,depth+1),right=Build(split,start+count-split,depth+1);nodes_[id]={b,0,0,left,right};return id;
}
size_t Mesh::Bytes()const{return triangles_.capacity()*sizeof(Triangle)+order_.capacity()*sizeof(uint32_t)+nodes_.capacity()*sizeof(Node);}
std::optional<V3> Mesh::Ray(V3 o,V3 d,double maxDistance)const{
    Check(Valid(o)&&Valid(d)&&std::isfinite(maxDistance)&&maxDistance>=0,"Invalid mesh ray");double length=d.Length();if(length<1e-10)return {};d=d*(1/length);
    double best=maxDistance;bool found=false;std::vector<uint32_t> stack{0};
    while(!stack.empty()){
        auto n=nodes_[stack.back()];stack.pop_back();if(!RayBox(n.bounds,o,d,best))continue;
        if(n.count){for(uint32_t k=n.start;k<n.start+n.count;k++){const auto &t=triangles_[order_[k]];double hit=RayTriangle(o,d,V(t.a),V(t.b),V(t.c));if(hit>=0&&hit<=best){best=hit;found=true;}}}
        else {stack.push_back(n.right);stack.push_back(n.left);}
    }return found?std::optional<V3>(o+d*best):std::nullopt;
}
bool Mesh::Deepest(V3 center,double half,double radius,V3 &push)const{
    Box area{center-V3{radius,half+radius,radius},center+V3{radius,half+radius,radius}};double best=1e-4;
    std::vector<uint32_t> stack{0};
    while(!stack.empty()){
        auto n=nodes_[stack.back()];stack.pop_back();if(!Overlap(n.bounds,area))continue;
        if(n.count){for(uint32_t k=n.start;k<n.start+n.count;k++){
            const auto &t=triangles_[order_[k]];V3 sp=center,tp;
            if(half>0)ClosestSegment(center-V3{0,half,0},center+V3{0,half,0},V(t.a),V(t.b),V(t.c),sp,tp);else tp=Closest(center,V(t.a),V(t.b),V(t.c));
            V3 delta=sp-tp;double distance=delta.Length(),penetration=radius-distance;
            if(penetration>best){best=penetration;push=distance>1e-10?delta*(penetration/distance):V(t.n)*penetration;}
        }}else{stack.push_back(n.right);stack.push_back(n.left);}
    }return best>1e-4;
}
bool Mesh::Free(V3 p)const{V3 ignored;return !Deepest(p,0,Resolution()*.5,ignored);}
std::shared_ptr<Mesh> Mesh::Load(const std::string &path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);Check(bool(f),"Cannot open GLB collision resource");auto size=f.tellg();Check(size>=20&&uint64_t(size)<=MaxBytes,"GLB collision resource exceeds 64 MiB or is truncated");
    std::vector<uint8_t> bytes(size);f.seekg(0);Check(bool(f.read(reinterpret_cast<char*>(bytes.data()),size)),"Truncated GLB collision");
    Check(U32(bytes.data())==0x46546c67&&U32(bytes.data()+4)==2&&U32(bytes.data()+8)==bytes.size(),"Invalid GLB collision header");
    Json j;std::vector<uint8_t> bin;bool haveJson=false,haveBin=false;
    for(size_t p=12;p<bytes.size();){
        Check(bytes.size()-p>=8,"Truncated GLB chunk");size_t n=U32(bytes.data()+p);uint32_t type=U32(bytes.data()+p+4);p+=8;
        Check(n%4==0&&n<=bytes.size()-p,"Invalid GLB chunk length");
        if(type==0x4e4f534a){Check(!haveJson&&p==20&&n<=4*1024*1024,"Invalid GLB JSON chunk");j=Json::parse(bytes.begin()+p,bytes.begin()+p+n,[](int depth,Json::parse_event_t,Json&){Check(depth<=64,"GLB JSON nesting exceeds limit");return true;});haveJson=true;}
        else if(type==0x004e4942){Check(haveJson&&!haveBin,"Invalid GLB binary chunk");bin.assign(bytes.begin()+p,bytes.begin()+p+n);haveBin=true;}
        p+=n;
    }
    Check(haveJson&&haveBin&&j.at("asset").value("version",std::string())=="2.0","GLB collision requires glTF 2.0 and embedded geometry");
    Check(!j.contains("extensionsRequired")||j.at("extensionsRequired").empty(),"Required compressed/quantized GLB collision extensions are not supported");
    const auto &buffers=j.at("buffers");Check(buffers.is_array()&&buffers.size()==1&&!buffers[0].contains("uri"),"External GLB collision buffers are not supported");
    size_t binLength=Integer(buffers[0].at("byteLength"));Check(binLength<=bin.size()&&bin.size()-binLength<=3,"GLB binary length mismatch");bin.resize(binLength);
    const auto &meshes=j.at("meshes");Check(meshes.is_array()&&meshes.size()<=4096,"Invalid GLB collision meshes");std::vector<V3> positions;std::vector<uint32_t> indices;
    // Like the fixed upstream fromGlb, consume mesh resources once, in baked
    // positions. Node transforms/skins are not applied to collision geometry.
    for(const auto &mesh:meshes)for(const auto &primitive:mesh.at("primitives")){
        Check(Optional(primitive,"mode",4)==4&&!primitive.contains("extensions")&&!primitive.contains("targets"),"Only static triangle GLB collision primitives are supported");
        auto a=GetAccessor(j,bin,Integer(primitive.at("attributes").at("POSITION")),true);
        Check(positions.size()+a.count<=MaxTriangles*3,"Too many GLB collision vertices");const size_t base=positions.size();
        for(size_t i=0;i<a.count;i++){V3 v;for(int k=0;k<3;k++){auto bits=U32(a.data+i*a.stride+k*4);float value;std::memcpy(&value,&bits,4);v[k]=value;}Check(Valid(v),"Invalid GLB collision position");positions.push_back(v);}
        if(primitive.contains("indices")){
            auto aIndex=GetAccessor(j,bin,Integer(primitive.at("indices")),false);Check(aIndex.count%3==0&&indices.size()+aIndex.count<=MaxTriangles*3,"Invalid GLB triangle list");
            for(size_t i=0;i<aIndex.count;i++){auto p=aIndex.data+i*aIndex.stride;uint32_t v=aIndex.component==5121?p[0]:aIndex.component==5123?uint32_t(p[0])|(uint32_t(p[1])<<8):U32(p);Check(v<a.count,"GLB index exceeds vertex count");indices.push_back(uint32_t(base+v));}
        }else{Check(a.count%3==0&&indices.size()+a.count<=MaxTriangles*3,"Invalid nonindexed GLB triangles");for(size_t i=0;i<a.count;i++)indices.push_back(uint32_t(base+i));}
    }
    return std::make_shared<Mesh>(positions,indices);
}
}
