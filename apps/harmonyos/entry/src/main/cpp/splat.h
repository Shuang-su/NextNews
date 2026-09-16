#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cmath>

namespace splat {
constexpr size_t MaxGaussians = 4000000; // Per source file.
constexpr size_t MaxDrawGaussians = 8000000; // Encoded streaming experiment only.
constexpr size_t MaxFileBytes = 128 * 1024 * 1024;
struct Gaussian {
    float position[3];
    float color[4];
    float covariance[6]; // xx, xy, xz, yy, yz, zz
};
// Lossless SOG byte codes; only centers are expanded on the streaming loader.
struct SogCodes { uint32_t quaternion=0,scale=0,color=0; };
struct SogTables { std::array<float,256> scale{},color{}; bool viewerTransform=false; };
inline Gaussian DecodeSog(const float *position,const SogCodes &codes,const SogTables &tables) {
    Gaussian g{};std::copy_n(position,3,g.position);
    for(int k=0;k<3;++k)g.color[k]=std::clamp(.5f+.28209479177387814f*tables.color[(codes.color>>(k*8))&255],0.f,1.f);
    g.color[3]=(codes.color>>24)/255.f;
    float q[4]{};float sum=0;const auto mode=codes.quaternion>>24;
    if(mode<252)throw std::runtime_error("SOG quaternion mode");
    for(int k=0,j=0;k<4;++k)if(k!=int(mode-252)){q[k]=(((codes.quaternion>>(j++*8))&255)/255.f-.5f)*1.41421356237f;sum+=q[k]*q[k];}
    q[mode-252]=std::sqrt(std::max(0.f,1-sum));
    const float inv=1/std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    for(auto &v:q)v*=inv;const float w=q[0],x=q[1],y=q[2],z=q[3];
    const float r[3][3]={{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)}, {2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)}, {2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}};
    float cov[3][3]{};
    for(int k=0;k<3;++k){const float s=std::exp(2*tables.scale[(codes.scale>>(k*8))&255]);for(int u=0;u<3;++u)for(int v=0;v<3;++v)cov[u][v]+=r[u][k]*r[v][k]*s;}
    const float packed[]={cov[0][0],cov[0][1],cov[0][2],cov[1][1],cov[1][2],cov[2][2]};std::copy_n(packed,6,g.covariance);
    if(tables.viewerTransform){g.covariance[2]=-g.covariance[2];g.covariance[4]=-g.covariance[4];}return g;
}
struct Scene;
struct SceneRange { std::shared_ptr<const Scene> source; uint32_t offset,count,logical; };
struct Scene {
    std::vector<Gaussian> points;
    // Optional coefficient-major RGB: DC color then 15 SH vectors. SH0 scenes allocate none.
    std::vector<std::array<float,48>> harmonics;
    int shDegree=0;
    bool shTransform=false;
    bool paged=false;
    std::shared_ptr<SogTables> tables;
    std::vector<SogCodes> codes;
    std::vector<std::array<float,3>> positions;
    std::vector<uint32_t> addresses;
    std::vector<SceneRange> ranges;
    size_t Count()const{return paged||tables?positions.size():points.size();}
    const float *Position(size_t i)const{return paged||tables?positions[i].data():points[i].position;}
    Gaussian At(size_t i)const{
        if(tables)return DecodeSog(positions.at(i).data(),codes.at(i),*tables);
        if(!paged)return points.at(i);
        auto r=std::upper_bound(ranges.begin(),ranges.end(),i,[](size_t value,const SceneRange &range){return value<range.logical;});
        if(r==ranges.begin())throw std::out_of_range("Invalid page range");--r;
        return r->source->At(r->offset+i-r->logical);
    }
    std::array<float, 3> center{};
    float radius = 1;
    std::array<float,6> worldBox{};
    bool hasWorldBox=false;
    float clippingRadius=0;
    void Include(const float *p){
        if(!hasWorldBox){for(int k=0;k<3;++k)worldBox[k]=worldBox[k+3]=p[k];hasWorldBox=true;}
        else for(int k=0;k<3;++k){worldBox[k]=std::min(worldBox[k],p[k]);worldBox[k+3]=std::max(worldBox[k+3],p[k]);}
    }
    void FitClipping(){
        if(!hasWorldBox)return;double square=0;
        for(int k=0;k<3;++k){double d=std::max(std::abs(double(center[k])-worldBox[k]),std::abs(double(center[k])-worldBox[k+3]));square+=d*d;}
        clippingRadius=std::max(radius,float(std::sqrt(square)));
    }
};
Scene ReadSog(const std::string &path, const std::atomic<bool> *cancel = nullptr,bool encoded=false);
Scene ReadModel(const std::string &path, const std::atomic<bool> *cancel = nullptr,bool encoded=false);
std::array<float,4> InspectModel(const std::string &path);
void ApplyViewerTransform(Scene &scene);
Scene ReadPly(const std::string &path, const std::atomic<bool> *cancel = nullptr);
struct Camera {
    float yaw = 0, pitch = 0, zoom = 1, panX = 0, panY = 0, panZ = 0, fly = 0, fov = 45;
};
struct View {
    std::array<float, 16> matrix;
    float nearPlane, farPlane, tanHalfFov = .41421356f;
};
View MakeView(const Scene &scene, const Camera &camera);
std::vector<float> Pick(const Scene &scene, const View &view, float x, float y, int width, int height);
std::vector<uint32_t> SortIndices(const Scene &scene, const View &view);
std::vector<Gaussian> Sort(const Scene &scene, const View &view);
}
