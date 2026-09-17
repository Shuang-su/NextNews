#include "collision.h"
#include "debug.h"
#include "../third_party/nlohmann/json.hpp"
#include <fstream>
#include <limits>
#include <stdexcept>
namespace viewer {
namespace {
constexpr double Epsilon=1e-4,Dt=1.0/60.0;
bool Finite(V3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<=1e6&&std::abs(p.y)<=1e6&&std::abs(p.z)<=1e6;}
void Require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int Pop(uint32_t n){return __builtin_popcount(n);}
Box Shape(V3 p,double half,double r){return {{p.x-r,p.y-half-r,p.z-r},{p.x+r,p.y+half+r,p.z+r}};}
std::vector<uint8_t> Read(const std::string &path,size_t limit){
    std::ifstream f(path,std::ios::binary|std::ios::ate);Require(bool(f),"Cannot open collision resource");
    auto size=f.tellg();Require(size>=0&&uint64_t(size)<=limit,"Collision resource too large");std::vector<uint8_t> b(size);f.seekg(0);
    Require(!size||bool(f.read(reinterpret_cast<char*>(b.data()),size)),"Truncated collision resource");return b;
}
V3 Vector(const nlohmann::json &a){Require(a.is_array()&&a.size()==3,"Invalid collision bounds");V3 p{a[0].get<double>(),a[1].get<double>(),a[2].get<double>()};Require(Finite(p),"Nonfinite collision bounds");return p;}
}
bool Collision::Capsule(V3 center,double half,double radius,V3 &out)const {
    Require(Finite(center)&&std::isfinite(half)&&half>=0&&half<=10&&std::isfinite(radius)&&radius>0&&radius<=10,"Invalid collision shape");
    V3 total{},normals[3];int count=0;bool hit=false;
    for(int i=0;i<4;i++){
        V3 original;if(!Deepest(center+total,half,radius,original))break;
        hit=true;V3 push=original;
        for(int n=0;n<count;n++){double dot=push.Dot(normals[n]);if(dot<0)push=push-normals[n]*dot;}
        const double len=original.Length();if(len>Epsilon&&count<3)normals[count++]=original*(1/len);
        total=total+push;
    }
    if(hit&&total.Dot(total)>Epsilon*Epsilon){out=total;return true;}return false;
}
Voxel::Voxel(Box grid,double res,int depth,bool flip,std::vector<uint32_t> nodes,std::vector<uint32_t> leaves):
    grid_(grid),resolution_(res),depth_(depth),flip_(flip),nodes_(std::move(nodes)),leaves_(std::move(leaves)){
    Require(Finite(grid.min)&&Finite(grid.max)&&std::isfinite(res)&&res>=.005&&res<=10&&depth>=0&&depth<=16,"Invalid voxel dimensions");
    for(int i=0;i<3;i++){
        const double count=(grid.max[i]-grid.min[i])/res;
        Require(count>0&&std::round(count)<=double(4u<<depth)&&std::abs(count-std::round(count))<.01,"Invalid voxel grid size");dimensions_[i]=int(std::round(count));
        grid_.max[i]=grid_.min[i]+dimensions_[i]*res;
    }
    Require(nodes_.size()<=0xffffff&&leaves_.size()<=0x2000000&&leaves_.size()%2==0,"Invalid voxel buffers");
    // Validate before any query: forward BFS children, bounded leaves, no cycles.
    for(size_t i=0;i<nodes_.size();i++){
        const uint32_t n=nodes_[i];if(n==0xff000000)continue;
        const uint32_t mask=n>>24,base=n&0xffffff;
        if(mask)Require(base>i&&uint64_t(base)+Pop(mask)<=nodes_.size(),"Invalid octree child reference");
        else Require(uint64_t(base)*2+1<leaves_.size(),"Invalid octree leaf reference");
    }
    if(!nodes_.empty()){
        std::vector<std::pair<uint32_t,int>> todo{{0,depth_}};std::vector<bool> seen(nodes_.size());
        while(!todo.empty()){
            auto [i,level]=todo.back();todo.pop_back();Require(!seen[i],"Shared octree child");seen[i]=true;
            auto n=nodes_[i],mask=n>>24;if(n==0xff000000||!mask)continue;
            Require(level>0,"Octree exceeds declared depth");
            for(int j=0;j<Pop(mask);j++)todo.push_back({(n&0xffffff)+uint32_t(j),level-1});
        }
    }
}
std::shared_ptr<Voxel> Voxel::Load(const std::string &metaPath,const std::string &binPath,int coordinateSpace){
    Require(coordinateSpace>=-1&&coordinateSpace<=1,"Invalid voxel coordinate space");
    auto data=Read(metaPath,1024*1024);auto m=nlohmann::json::parse(data);
    Require(m.value("leafSize",0)==4,"Unsupported voxel leaf size");
    std::string version=m.value("version",std::string("1.0"));Require(version=="1.0"||version=="1.1","Unsupported voxel version");
    Require(m.at("nodeCount").is_number_integer()&&m.at("leafDataCount").is_number_integer()&&m.at("treeDepth").is_number_integer(),"Collision counts must be integers");
    int64_t nc=m.at("nodeCount").get<int64_t>(),lc=m.at("leafDataCount").get<int64_t>();
    Require(nc>=0&&lc>=0&&nc+lc<=16*1024*1024&&lc%2==0,"Invalid voxel counts");
    auto bytes=Read(binPath,64*1024*1024);Require(bytes.size()==uint64_t(nc+lc)*4,"Voxel binary size mismatch");
    std::vector<uint32_t> nodes(nc),leaves(lc);
    for(size_t i=0;i<bytes.size()/4;i++){
        auto p=bytes.data()+i*4;uint32_t v=uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
        if(i<size_t(nc))nodes[i]=v;else leaves[i-nc]=v;
    }
    Box grid{Vector(m.at("gridBounds").at("min")),Vector(m.at("gridBounds").at("max"))};
    return std::make_shared<Voxel>(grid,m.at("voxelResolution").get<double>(),m.at("treeDepth").get<int>(),(coordinateSpace<0?version=="1.0":coordinateSpace==1),std::move(nodes),std::move(leaves));
}
bool Voxel::Solid(int x,int y,int z)const {
    if(nodes_.empty()||!Inside(x,y,z))return false;
    uint32_t index=0;const int bx=x/4,by=y/4,bz=z/4;
    for(int level=depth_-1;level>=0;level--){
        const auto n=nodes_[index];if(n==0xff000000)return true;const auto mask=n>>24;if(!mask)break;
        const auto octant=(((bz>>level)&1)<<2)|(((by>>level)&1)<<1)|((bx>>level)&1);
        if(!(mask&(1u<<octant)))return false;index=(n&0xffffff)+Pop(mask&((1u<<octant)-1));
    }
    const auto n=nodes_[index];if(n==0xff000000)return true;
    const uint32_t bit=(z&3)*16+(y&3)*4+(x&3);return (leaves_[(n&0xffffff)*2+bit/32]>>(bit%32))&1;
}
Box Voxel::Bounds()const {return flip_?Box{{-grid_.max.x,-grid_.max.y,grid_.min.z},{-grid_.min.x,-grid_.min.y,grid_.max.z}}:grid_;}
void Voxel::Debug(Box area,DebugWire &wire)const {
    if(!Available()||!DebugIntersects(area,Bounds()))return;
    if(flip_)area={{-area.max.x,-area.max.y,area.min.z},{-area.min.x,-area.min.y,area.max.z}};
    const V3 center=(area.min+area.max)*.5;
    struct Task{uint32_t index;int size;V3 origin;};
    std::vector<Task> stack{{0,4<<depth_,grid_.min}};
    auto world=[this](Box b){return flip_?Box{{-b.max.x,-b.max.y,b.min.z},{-b.min.x,-b.min.y,b.max.z}}:b;};
    while(!stack.empty()){
        auto t=stack.back();stack.pop_back();if(!wire.Visit())return;
        const double span=t.size*resolution_;Box box{t.origin,t.origin+V3{span,span,span}};
        for(int k=0;k<3;k++)box.max[k]=std::min(box.max[k],grid_.max[k]);
        if(box.max.x<=box.min.x||box.max.y<=box.min.y||box.max.z<=box.min.z)continue;
        if(!DebugIntersects(area,box))continue;
        auto node=nodes_[t.index],mask=node>>24;
        if(node==0xff000000){wire.Cube(0,world(box));continue;}
        if(mask){
            std::vector<Task> children;
            for(int i=0;i<8;i++)if(mask&(1u<<i)){V3 p=t.origin+V3{double(i&1),double((i>>1)&1),double((i>>2)&1)}*(span*.5);children.push_back({(node&0xffffff)+uint32_t(Pop(mask&((1u<<i)-1))),t.size/2,p});}
            std::sort(children.begin(),children.end(),[&](const Task&a,const Task&b){double s=span*.5;return DebugDistance({a.origin,a.origin+V3{s,s,s}},center)>DebugDistance({b.origin,b.origin+V3{s,s,s}},center);});
            stack.insert(stack.end(),children.begin(),children.end());continue;
        }
        // Early leaf patterns repeat at 4-cell intervals, as in Solid().
        std::array<int,3> lo,hi;
        for(int k=0;k<3;k++){lo[k]=std::max(0,int(std::floor((std::max(area.min[k],box.min[k])-grid_.min[k])/resolution_)));hi[k]=std::min(dimensions_[k]-1,int(std::ceil((std::min(area.max[k],box.max[k])-grid_.min[k])/resolution_))-1);}
        for(int z=lo[2];z<=hi[2];z++)for(int y=lo[1];y<=hi[1];y++)for(int x=lo[0];x<=hi[0];x++){
            if(!wire.Visit())return;const auto bit=(z&3)*16+(y&3)*4+(x&3);
            if(!((leaves_[(node&0xffffff)*2+bit/32]>>(bit%32))&1))continue;
            V3 a=grid_.min+V3{double(x),double(y),double(z)}*resolution_;wire.Cube(0,world({a,a+V3{resolution_,resolution_,resolution_}}));
        }
    }
}
bool Voxel::Known(Box area)const {
    auto b=Bounds();if(nodes_.empty())return false;
    for(int i=0;i<3;i++)if(area.min[i]<b.min[i]||area.max[i]>=b.max[i])return false;
    return true;
}
bool Voxel::Free(V3 p)const {
    if(!Finite(p)||nodes_.empty())return false;p=Transform(p);if(!grid_.Contains(p))return false;
    return !Solid(int(std::floor((p.x-grid_.min.x)/resolution_)),int(std::floor((p.y-grid_.min.y)/resolution_)),int(std::floor((p.z-grid_.min.z)/resolution_)));
}
std::optional<V3> Voxel::Ray(V3 origin,V3 direction,double maxDistance)const {
    Require(Finite(origin)&&Finite(direction)&&std::isfinite(maxDistance)&&maxDistance>=0&&maxDistance<=10000,"Invalid collision ray");
    double len=direction.Length();if(nodes_.empty()||len<1e-12)return {};
    direction=Transform(direction)*(1/len);origin=Transform(origin);
    double near=0,far=maxDistance;
    for(int i=0;i<3;i++){
        if(std::abs(direction[i])<1e-12){if(origin[i]<grid_.min[i]||origin[i]>=grid_.max[i])return {};}
        else{double a=(grid_.min[i]-origin[i])/direction[i],b=(grid_.max[i]-origin[i])/direction[i];if(a>b)std::swap(a,b);near=std::max(near,a);far=std::min(far,b);if(near>far)return {};}
    }
    V3 p=origin+direction*near;std::array<int,3> cell,step;V3 next,delta;
    for(int i=0;i<3;i++){
        cell[i]=std::clamp(int(std::floor((p[i]-grid_.min[i])/resolution_)),0,dimensions_[i]-1);
        step[i]=direction[i]>0?1:-1;
        if(std::abs(direction[i])<1e-12){next[i]=delta[i]=std::numeric_limits<double>::infinity();}
        else{next[i]=(grid_.min[i]+(cell[i]+(step[i]>0?1:0))*resolution_-origin[i])/direction[i];delta[i]=resolution_/std::abs(direction[i]);}
    }
    double t=near;
    while(t<=far&&Inside(cell[0],cell[1],cell[2])){
        if(Solid(cell[0],cell[1],cell[2]))return Transform(origin+direction*t);
        int axis=next.x<next.y?(next.x<next.z?0:2):(next.y<next.z?1:2);t=next[axis];next[axis]+=delta[axis];cell[axis]+=step[axis];
    }return {};
}
bool Voxel::Deepest(V3 center,double half,double radius,V3 &out)const {
    center=Transform(center);const double bottom=center.y-half,top=center.y+half;
    auto box=Shape(center,half,radius);std::array<int,3> lo,hi;uint64_t tests=1;
    for(int i=0;i<3;i++)if(box.max[i]<grid_.min[i]||box.min[i]>grid_.max[i])return false;
    for(int i=0;i<3;i++){lo[i]=int(std::clamp(std::floor((box.min[i]-grid_.min[i])/resolution_),0.0,double(dimensions_[i]-1)));hi[i]=int(std::clamp(std::floor((box.max[i]-grid_.min[i])/resolution_),0.0,double(dimensions_[i]-1)));tests*=uint64_t(std::max(0,hi[i]-lo[i]+1));}
    Require(tests<=2000000,"Collision query exceeds work limit");
    double best=Epsilon;V3 push{};
    for(int z=lo[2];z<=hi[2];z++)for(int y=lo[1];y<=hi[1];y++)for(int x=lo[0];x<=hi[0];x++){
        if(!Solid(x,y,z))continue;
        V3 min=grid_.min+V3{double(x),double(y),double(z)}*resolution_,max=min+V3{resolution_,resolution_,resolution_};
        double sy=top<min.y?top:bottom>max.y?bottom:std::clamp((min.y+max.y)*.5,bottom,top);
        V3 c{center.x,sy,center.z},nearest;
        for(int i=0;i<3;i++)nearest[i]=std::clamp(c[i],min[i],max[i]);
        V3 d=c-nearest;double sq=d.Dot(d);if(sq>=radius*radius)continue;
        V3 candidate;double penetration;
        if(sq>1e-12){double dist=std::sqrt(sq);penetration=radius-dist;candidate=d*(penetration/dist);}
        else{
            V3 escape;for(int i=0;i<3;i++){double neg=c[i]-min[i],pos=max[i]-c[i];escape[i]=neg<pos?-(neg+radius):pos+radius;}
            int axis=std::abs(escape.x)<=std::abs(escape.y)&&std::abs(escape.x)<=std::abs(escape.z)?0:std::abs(escape.y)<=std::abs(escape.z)?1:2;
            candidate[axis]=escape[axis];penetration=std::abs(escape[axis]);
        }
        if(penetration>best){best=penetration;push=candidate;}
    }
    if(best>Epsilon){out=Transform(push);return true;}return false;
}
std::optional<V3> FindSpawn(const Collision &c,V3 origin,double half,double radius){
    Require(Finite(origin)&&half>0&&half<5&&radius>0&&radius<5,"Invalid spawn query");
    double step=c.Resolution();int cells=int(std::ceil(5/step)),feet=int(std::ceil(radius/step));
    double best=std::numeric_limits<double>::infinity();std::optional<V3> result;uint64_t candidates=0;
    for(int r=0;r<=cells&&r*step*r*step<best;r++)for(int y=-r;y<=r;y++)for(int z=-r;z<=r;z++)for(int x=-r;x<=r;x++){
        if(std::abs(x)<r&&std::abs(y)<r&&std::abs(z)<r)continue;
        Require(++candidates<=2000000,"Spawn search work limit exceeded");
        double ds=double(x*x+y*y+z*z)*step*step;if(ds>=best||ds>25)continue;
        V3 p=origin+V3{double(x),double(y),double(z)}*step;if(!c.Free(p))continue;
        double floor=-std::numeric_limits<double>::infinity(),ceiling=-floor;bool supported=true;
        for(int i=-feet;i<=feet&&supported;i++)for(int j=-feet;j<=feet;j++){
            if((i*i+j*j)*step*step>radius*radius)continue;
            V3 q=p+V3{i*step,0,j*step};auto down=c.Ray(q,{0,-1,0},1000);if(!down){supported=false;break;}
            floor=std::max(floor,down->y);auto up=c.Ray(q,{0,1,0},1000);if(up)ceiling=std::min(ceiling,up->y);
        }
        if(!supported||floor+2*half>ceiling)continue;
        // Extra streaming safety compared with upstream: the full placed body
        // must be covered, even if the floor ray hit a ready neighbouring tile.
        if(!c.Known(Shape({p.x,floor+half,p.z},half-radius,radius)))continue;
        best=ds;result=V3{p.x,floor,p.z};
    }
    return result;
}
bool Walker::Enter(const Collision &c,V3 eye){
    ready_=false;auto floor=FindSpawn(c,eye);if(!floor)return false;
    spawn_={floor->x,floor->y+1.5,floor->z};ready_=true;Reset();return true;
}
void Walker::Reset(){position_=previous_=spawn_;Pause();grounded_=ready_;jumping_=false;blocked_=false;}
WalkResult Walker::Update(const Collision &c,double seconds,double yaw,double right,double forward,bool jump){
    Require(std::isfinite(seconds)&&seconds>=0&&std::isfinite(yaw)&&std::isfinite(right)&&std::isfinite(forward),"Invalid walking input");
    if(!ready_)return {position_,false,true};
    seconds=std::min(seconds,10*Dt);accumulator_=std::min(accumulator_+seconds,10*Dt);
    // Official input controller uses 4 units/second; input here is integrated by caller.
    pending_.x+=right;pending_.z+=forward;pending_.y=pending_.y||jump;
    const int steps=int(accumulator_/Dt);
    if(steps){V3 move=pending_*(1.0/steps);for(int i=0;i<steps;i++){previous_=position_;Step(c,yaw,move,pending_.y!=0);accumulator_-=Dt;}pending_={};}
    return {previous_+(position_-previous_)*(accumulator_/Dt),grounded_,blocked_};
}
void Walker::Step(const Collision &c,double yaw,V3 move,bool jump){
    blocked_=false;const V3 original=position_;const bool previousGrounded=grounded_;
    if(!c.Known(Shape({position_.x,position_.y-.55,position_.z},.55,.21))){velocity_={};blocked_=true;return;}
    double ground=0;int hits=0;
    for(int i=0;i<5;i++){V3 o{position_.x,position_.y-1.3,position_.z};if(i==1)o.x-=.2;else if(i==2)o.x+=.2;else if(i==3)o.z-=.2;else if(i==4)o.z+=.2;auto hit=c.Ray(o,{0,-1,0},1);if(hit){ground+=hit->y;hits++;}}
    if(velocity_.y<0)jumping_=false;
    if(jump&&!jumping_&&grounded_&&!jumpHeld_){jumping_=true;velocity_.y=4;grounded_=false;}jumpHeld_=jump;
    if(hits&&!jumping_){
        double target=ground/hits+1.5,displacement=position_.y-target;
        if(displacement>.1){velocity_.y-=9.8*Dt;if(position_.y+velocity_.y*Dt<=target){position_.y=target;velocity_.y=0;}grounded_=false;}
        else{velocity_.y+=(-800*displacement-57*velocity_.y)*Dt;grounded_=true;}
    }else{velocity_.y-=9.8*Dt;grounded_=false;}
    const double sy=std::sin(yaw),cy=std::cos(yaw),speed=grounded_?7:1;
    velocity_.x+=(cy*move.x-sy*move.z)*speed;velocity_.z+=(-sy*move.x-cy*move.z)*speed;
    const double factor=std::pow(grounded_?.99:.998,Dt*1000);velocity_.x*=factor;velocity_.z*=factor;
    const V3 delta=velocity_*Dt;const int subdivisions=std::max(1,int(std::ceil(delta.Length()/std::min(.1,c.Resolution()*.5))));
    if(subdivisions>128){position_=original;velocity_={};blocked_=true;return;}
    for(int i=0;i<subdivisions;i++){
        position_=position_+delta*(1.0/subdivisions);V3 center{position_.x,position_.y-.55,position_.z};
        if(!c.Known(Shape(center,.55,.21))){position_=original;velocity_={};grounded_=previousGrounded;blocked_=true;return;}
        V3 push;if(c.Capsule(center,.55,.2,push)){
            position_=position_+push;if(push.y<0&&velocity_.y>0)velocity_.y=0;
            if(!grounded_&&push.y>0&&velocity_.y<0){velocity_.y=0;grounded_=true;}
            if(!c.Known(Shape({position_.x,position_.y-.55,position_.z},.55,.21))){position_=original;velocity_={};grounded_=previousGrounded;blocked_=true;return;}
        }
    }
}
}
