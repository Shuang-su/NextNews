#pragma once
// Algorithms adapted from SuperSplat Viewer v1.31.2 (MIT, PlayCanvas Ltd.).
// See docs/harmonyos/THIRD_PARTY_NOTICES.md. Renderer-independent world coordinates.
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <algorithm>
#include <optional>
#include <cstdint>
namespace viewer {
struct V3 {
    double x=0,y=0,z=0;
    double &operator[](size_t i){return i==0?x:i==1?y:z;}
    double operator[](size_t i)const{return i==0?x:i==1?y:z;}
    V3 operator+(V3 b)const{return {x+b.x,y+b.y,z+b.z};}
    V3 operator-(V3 b)const{return {x-b.x,y-b.y,z-b.z};}
    V3 operator*(double s)const{return {x*s,y*s,z*s};}
    double Dot(V3 b)const{return x*b.x+y*b.y+z*b.z;}
    double Length()const{return std::sqrt(Dot(*this));}
};
struct Box {
    V3 min,max;
    bool Contains(V3 p)const{return p.x>=min.x&&p.y>=min.y&&p.z>=min.z&&p.x<max.x&&p.y<max.y&&p.z<max.z;}
};
class Collision {
public:
    virtual ~Collision()=default;
    virtual std::optional<V3> Ray(V3 origin,V3 direction,double maxDistance)const=0;
    virtual bool Deepest(V3 center,double halfHeight,double radius,V3 &push)const=0;
    virtual bool Free(V3 point)const=0;
    // Coverage includes the entire shape and motion path; unknown is never free.
    virtual bool Known(Box area)const=0;
    virtual double Resolution()const=0;
    bool Capsule(V3 center,double halfHeight,double radius,V3 &push)const;
    bool Sphere(V3 center,double radius,V3 &push)const{return Capsule(center,0,radius,push);}
};
class Voxel final:public Collision {
public:
    static std::shared_ptr<Voxel> Load(const std::string &metadata,const std::string &binary);
    Voxel(Box grid,double resolution,int depth,bool flip,std::vector<uint32_t> nodes,std::vector<uint32_t> leaves);
    bool Solid(int x,int y,int z)const;
    bool Free(V3 point)const override;
    bool Known(Box area)const override;
    double Resolution()const override{return resolution_;}
    std::optional<V3> Ray(V3 origin,V3 direction,double maxDistance)const override;
    bool Deepest(V3 center,double halfHeight,double radius,V3 &push)const override;
    size_t Bytes()const{return (nodes_.size()+leaves_.size())*4;}
    Box Bounds()const;
    bool Available()const{return !nodes_.empty();}
private:
    V3 Transform(V3 p)const{return flip_?V3{-p.x,-p.y,p.z}:p;}
    bool Inside(int x,int y,int z)const{return x>=0&&y>=0&&z>=0&&x<dimensions_[0]&&y<dimensions_[1]&&z<dimensions_[2];}
    Box grid_;double resolution_;int depth_;bool flip_;
    std::array<int,3> dimensions_;
    std::vector<uint32_t> nodes_,leaves_;
};
// Closest supported cylinder within the official five metre spawn search.
// Runs on a worker, not the ArkUI thread. Output is the floor position.
std::optional<V3> FindSpawn(const Collision &collision,V3 origin,double halfHeight=.85,double radius=.2);
struct WalkResult {V3 eye;bool grounded=false,blocked=false;};
class Walker {
public:
    bool Enter(const Collision &collision,V3 eye);
    WalkResult Update(const Collision &collision,double seconds,double yaw,double right,double forward,bool jump);
    void Reset();
    void Pause(){velocity_={};pending_={};accumulator_=0;jumpHeld_=false;}
    bool Ready()const{return ready_;}
    V3 Eye()const{return position_;}
private:
    void Step(const Collision&,double yaw,V3 move,bool jump);
    V3 position_,previous_,spawn_,velocity_,pending_;
    double accumulator_=0;bool ready_=false,grounded_=false,jumping_=false,jumpHeld_=false,blocked_=false;
};
}
