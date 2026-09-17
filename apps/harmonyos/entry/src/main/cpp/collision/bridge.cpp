#include "bridge.h"
#include "atlas.h"
#include "mesh.h"
#include "debug.h"
#include <mutex>
#include <functional>
#include <atomic>
using namespace viewer;
namespace {
std::mutex stateMutex;
CollisionCache cache;
Walker walker;
uint64_t generation=0,spawnRevision=0;
std::atomic<int> pendingLoads{0};
napi_value Undefined(napi_env e){napi_value v;napi_get_undefined(e,&v);return v;}
napi_value Number(napi_env e,double x){napi_value v;napi_create_double(e,x,&v);return v;}
napi_value Array(napi_env e,const std::vector<double> &values){napi_value a;napi_create_array_with_length(e,values.size(),&a);for(size_t i=0;i<values.size();i++)napi_set_element(e,a,i,Number(e,values[i]));return a;}
double Double(napi_env e,napi_value v){double n;if(napi_get_value_double(e,v,&n)!=napi_ok||!std::isfinite(n)||std::abs(n)>1e6)throw std::runtime_error("Invalid collision number");return n;}
std::string String(napi_env e,napi_value v){size_t length=0;if(napi_get_value_string_utf8(e,v,nullptr,0,&length)!=napi_ok||!length||length>4096)throw std::runtime_error("Invalid collision string");std::vector<char> b(length+1);napi_get_value_string_utf8(e,v,b.data(),b.size(),&length);std::string s(b.data(),length);if(s.find('\0')!=std::string::npos)throw std::runtime_error("Invalid collision string");return s;}
std::vector<napi_value> Args(napi_env e,napi_callback_info info,size_t count){std::vector<napi_value> a(count);size_t n=count;napi_get_cb_info(e,info,&n,a.data(),nullptr,nullptr);if(n!=count)throw std::runtime_error("Invalid collision argument count");return a;}
struct Work {napi_async_work work;napi_deferred deferred;std::function<std::vector<double>()> execute;std::vector<double> result;std::string error;};
napi_value Async(napi_env env,const char *label,std::function<std::vector<double>()> execute){
    auto *job=new Work{};job->execute=std::move(execute);napi_value promise,name;napi_create_promise(env,&job->deferred,&promise);napi_create_string_utf8(env,label,NAPI_AUTO_LENGTH,&name);
    napi_create_async_work(env,nullptr,name,[](napi_env,void *data){auto *j=static_cast<Work*>(data);try{j->result=j->execute();}catch(const std::exception &e){j->error=e.what();}},
        [](napi_env e,napi_status status,void *data){auto *j=static_cast<Work*>(data);if(status==napi_ok&&j->error.empty())napi_resolve_deferred(e,j->deferred,Array(e,j->result));else{napi_value text,error;napi_create_string_utf8(e,j->error.empty()?"Collision work cancelled":j->error.c_str(),NAPI_AUTO_LENGTH,&text);napi_create_error(e,nullptr,text,&error);napi_reject_deferred(e,j->deferred,error);}napi_delete_async_work(e,j->work);delete j;},job,&job->work);
    napi_queue_async_work(env,job->work);return promise;
}
napi_value Clear(napi_env e,napi_callback_info){std::lock_guard<std::mutex> lock(stateMutex);cache=CollisionCache();walker=Walker();spawnRevision++;return Number(e,++generation);}
napi_value LoadResource(napi_env e,napi_callback_info info,bool mesh){try{
    std::vector<napi_value> a(5);size_t count=5;napi_get_cb_info(e,info,&count,a.data(),nullptr,nullptr);
    if((mesh&&count!=3)||(!mesh&&count!=4&&count!=5))throw std::runtime_error("Invalid collision argument count");
    int space=-1;if(!mesh&&count==5){const double n=Double(e,a[4]);if(n!=std::floor(n)||n<-1||n>1)throw std::runtime_error("Invalid voxel coordinate space");space=int(n);}
    const auto g=Double(e,a[0]);auto id=String(e,a[1]),meta=String(e,a[2]),bin=mesh?std::string():String(e,a[3]);
    return Async(e,"LoadViewerCollision",[g,id,meta,bin,mesh,space](){
        if(pendingLoads.fetch_add(1)>=2){pendingLoads--;throw std::runtime_error("Two collision loaders already active");}
        struct Done{~Done(){pendingLoads--;}}done;
        {std::lock_guard<std::mutex> lock(stateMutex);if(g!=generation)throw std::runtime_error("Stale collision scene");}
        std::shared_ptr<const CollisionResource> tile;
        if(mesh)tile=Mesh::Load(meta);else tile=Voxel::Load(meta,bin,space);auto b=tile->Bounds();
        std::lock_guard<std::mutex> lock(stateMutex);if(g!=generation)throw std::runtime_error("Stale collision scene");
        if(!cache.Insert(id,tile))throw std::runtime_error("Collision cache is pinned or exceeds 128 MiB");
        return std::vector<double>{double(tile->Bytes()),b.min.x,b.min.y,b.min.z,b.max.x,b.max.y,b.max.z};
    });
}catch(const std::exception &x){napi_throw_error(e,nullptr,x.what());return Undefined(e);}}
napi_value Load(napi_env e,napi_callback_info info){return LoadResource(e,info,false);}
napi_value LoadMesh(napi_env e,napi_callback_info info){return LoadResource(e,info,true);}
napi_value Select(napi_env e,napi_callback_info info){try{
    auto a=Args(e,info,2);double g=Double(e,a[0]);bool isArray=false;uint32_t n=0;napi_is_array(e,a[1],&isArray);napi_get_array_length(e,a[1],&n);if(!isArray||n>64)throw std::runtime_error("Invalid selected collision tiles");
    std::set<std::string> ids;for(uint32_t i=0;i<n;i++){napi_value v;napi_get_element(e,a[1],i,&v);ids.insert(String(e,v));}
    std::lock_guard<std::mutex> lock(stateMutex);if(g==generation)cache.Select(std::move(ids));return Undefined(e);
}catch(const std::exception &x){napi_throw_error(e,nullptr,x.what());return Undefined(e);}}
napi_value Enter(napi_env e,napi_callback_info info){try{
    auto a=Args(e,info,4);double g=Double(e,a[0]);V3 p{Double(e,a[1]),Double(e,a[2]),Double(e,a[3])};Atlas snapshot;uint64_t rev;
    {std::lock_guard<std::mutex> lock(stateMutex);snapshot=cache.Snapshot();rev=++spawnRevision;}
    return Async(e,"FindViewerSpawn",[g,p,snapshot,rev](){Walker next;if(snapshot.tiles.empty()||!next.Enter(snapshot,p))throw std::runtime_error("No supported spawn in ready collision data");std::lock_guard<std::mutex> lock(stateMutex);if(g!=generation||rev!=spawnRevision)throw std::runtime_error("Stale spawn");walker=next;auto eye=walker.Eye();return std::vector<double>{eye.x,eye.y,eye.z};});
}catch(const std::exception &x){napi_throw_error(e,nullptr,x.what());return Undefined(e);}}
napi_value Step(napi_env e,napi_callback_info info){try{
    auto a=Args(e,info,6);double g=Double(e,a[0]),dt=Double(e,a[1]),yaw=Double(e,a[2]),right=Double(e,a[3]),forward=Double(e,a[4]);bool jump=false;if(napi_get_value_bool(e,a[5],&jump)!=napi_ok||dt<0||dt>.2||std::abs(right)>2||std::abs(forward)>2)throw std::runtime_error("Invalid walk input");
    std::lock_guard<std::mutex> lock(stateMutex);if(g!=generation||!walker.Ready())return Array(e,{});
    auto result=walker.Update(cache.Snapshot(),dt,yaw,right,forward,jump);return Array(e,{result.eye.x,result.eye.y,result.eye.z,result.grounded?1.0:0.0,result.blocked?1.0:0.0});
}catch(const std::exception &x){napi_throw_error(e,nullptr,x.what());return Undefined(e);}}
napi_value Pause(napi_env e,napi_callback_info){std::lock_guard<std::mutex> lock(stateMutex);spawnRevision++;walker.Pause();return Undefined(e);}
napi_value Debug(napi_env e,napi_callback_info info){try{
    auto a=Args(e,info,4);const auto g=Double(e,a[0]);V3 eye{Double(e,a[1]),Double(e,a[2]),Double(e,a[3])};Atlas snapshot;
    {std::lock_guard<std::mutex> lock(stateMutex);if(g!=generation)throw std::runtime_error("Stale collision debug scene");snapshot=cache.Snapshot();}
    return Async(e,"ViewerCollisionDebug",[g,eye,snapshot]()mutable{
        DebugWire wire;Box area{eye-V3{5,5,5},eye+V3{5,5,5}};
        std::sort(snapshot.tiles.begin(),snapshot.tiles.end(),[&](const auto&a,const auto&b){return DebugDistance(a->Bounds(),eye)<DebugDistance(b->Bounds(),eye);});
        for(const auto &tile:snapshot.tiles)wire.Cube(2,tile->Bounds());
        for(const auto &tile:snapshot.tiles){if(!wire.Visit())break;tile->Debug(area,wire);}
        {std::lock_guard<std::mutex> lock(stateMutex);if(g!=generation)throw std::runtime_error("Stale collision debug scene");}
        wire.values.insert(wire.values.begin(),wire.truncated?1.0:0.0);return wire.values;
    });
}catch(const std::exception &x){napi_throw_error(e,nullptr,x.what());return Undefined(e);}}
napi_value Reset(napi_env e,napi_callback_info){std::lock_guard<std::mutex> lock(stateMutex);walker.Reset();auto p=walker.Eye();return Array(e,{p.x,p.y,p.z});}
napi_value Status(napi_env e,napi_callback_info){std::lock_guard<std::mutex> lock(stateMutex);auto ids=cache.Ids();napi_value result,arr;napi_create_object(e,&result);napi_create_array_with_length(e,ids.size(),&arr);for(uint32_t i=0;i<ids.size();i++){napi_value v;napi_create_string_utf8(e,ids[i].c_str(),NAPI_AUTO_LENGTH,&v);napi_set_element(e,arr,i,v);}napi_set_named_property(e,result,"ids",arr);napi_set_named_property(e,result,"bytes",Number(e,cache.Bytes()));return result;}
}
void RegisterCollision(napi_env e,napi_value exports){napi_property_descriptor methods[]={
    {"collisionClear",nullptr,Clear,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"collisionLoadVoxel",nullptr,Load,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"collisionLoadMesh",nullptr,LoadMesh,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"collisionSelect",nullptr,Select,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"collisionEnter",nullptr,Enter,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"collisionStep",nullptr,Step,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"collisionPause",nullptr,Pause,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"collisionReset",nullptr,Reset,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"collisionDebug",nullptr,Debug,nullptr,nullptr,nullptr,napi_default,nullptr},
    {"collisionStatus",nullptr,Status,nullptr,nullptr,nullptr,napi_default,nullptr}
};napi_define_properties(e,exports,sizeof(methods)/sizeof(methods[0]),methods);}
