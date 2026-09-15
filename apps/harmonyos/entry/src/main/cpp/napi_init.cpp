#include "collision/bridge.h"
#include "lod_selection.h"
#include <cstring>
#include "renderer.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <napi/native_api.h>
#include <cmath>

namespace {
void Created(OH_NativeXComponent *component,void *window) {
    uint64_t w=1,h=1;OH_NativeXComponent_GetXComponentSize(component,window,&w,&h);
    splat::Renderer::Get().Start(window,int(w),int(h));
}
void Changed(OH_NativeXComponent *component,void *window) {
    uint64_t w=1,h=1;OH_NativeXComponent_GetXComponentSize(component,window,&w,&h);splat::Renderer::Get().Resize(int(w),int(h));
}
void Destroyed(OH_NativeXComponent *,void *) {splat::Renderer::Get().Stop();}
OH_NativeXComponent_Callback callbacks={Created,Changed,Destroyed,nullptr};
napi_value Undefined(napi_env env){napi_value result;napi_get_undefined(env,&result);return result;}
napi_value Load(napi_env env,napi_callback_info info) {
    napi_value arg;size_t argc=1;napi_get_cb_info(env,info,&argc,&arg,nullptr,nullptr);
    size_t length=0;
    if(argc!=1||napi_get_value_string_utf8(env,arg,nullptr,0,&length)!=napi_ok||length==0||length>4096){napi_throw_type_error(env,nullptr,"Expected a model path");return Undefined(env);}
    std::vector<char> path(length+1);napi_get_value_string_utf8(env,arg,path.data(),path.size(),&length);
    if(std::string(path.data()).size()!=length){napi_throw_type_error(env,nullptr,"Invalid path");return Undefined(env);}
    splat::Renderer::Get().Load(std::string(path.data(),length));return Undefined(env);
}
napi_value Camera(napi_env env,napi_callback_info info) {
    napi_value args[8];size_t argc=8;napi_get_cb_info(env,info,&argc,args,nullptr,nullptr);double v[8]={0,0,1,0,0,0,0,45};
    if(argc!=7&&argc!=8){napi_throw_type_error(env,nullptr,"Expected seven camera values and optional vertical FOV");return Undefined(env);}
    for(size_t i=0;i<argc;++i)if(napi_get_value_double(env,args[i],&v[i])!=napi_ok||!std::isfinite(v[i])||std::abs(v[i])>1e4){napi_throw_range_error(env,nullptr,"Invalid camera value");return Undefined(env);}
    if(v[7]<=1||v[7]>=179){napi_throw_range_error(env,nullptr,"Invalid FOV");return Undefined(env);}
    splat::Renderer::Get().SetCamera({float(v[0]),float(v[1]),float(v[2]),float(v[3]),float(v[4]),float(v[5]),float(v[6]),float(v[7])});return Undefined(env);
}
napi_value Optimize(napi_env env,napi_callback_info info) {
    napi_value arg;size_t argc=1;bool enabled=false;napi_get_cb_info(env,info,&argc,&arg,nullptr,nullptr);
    if(argc!=1||napi_get_value_bool(env,arg,&enabled)!=napi_ok){napi_throw_type_error(env,nullptr,"Expected boolean");return Undefined(env);}
    splat::Renderer::Get().SetOptimized(enabled);return Undefined(env);
}
splat::LodTree lodTree; // Accessed only by the ArkUI thread, registered once per scene.
template<class T> bool ReadBuffer(napi_env env,napi_value value,std::vector<T>& output,size_t max){
    size_t bytes=0;void *data=nullptr;
    if(napi_get_arraybuffer_info(env,value,&data,&bytes)!=napi_ok||bytes%sizeof(T)||bytes/sizeof(T)>max)return false;
    output.resize(bytes/sizeof(T));if(bytes)std::memcpy(output.data(),data,bytes);return true;
}
napi_value RegisterLods(napi_env env,napi_callback_info info){
    napi_value args[4];size_t argc=4;napi_get_cb_info(env,info,&argc,args,nullptr,nullptr);
    splat::LodTree tree;std::vector<double> bounds;double levels=0;
    if(argc!=4||!ReadBuffer(env,args[0],bounds,4)||bounds.size()!=4||!ReadBuffer(env,args[1],tree.boxes,60000)||tree.boxes.size()%6||!ReadBuffer(env,args[2],tree.lods,480000)||napi_get_value_double(env,args[3],&levels)!=napi_ok||!std::isfinite(levels)||levels<1||levels>16||levels!=std::floor(levels)||tree.lods.size()!=tree.boxes.size()/6*levels*3){napi_throw_type_error(env,nullptr,"Invalid LOD tree buffers");return Undefined(env);}
    for(auto v:bounds)if(!std::isfinite(v)||std::abs(v)>1e6){napi_throw_range_error(env,nullptr,"Invalid LOD tree bounds");return Undefined(env);}
    if(bounds[3]<=0){napi_throw_range_error(env,nullptr,"Invalid LOD radius");return Undefined(env);}
    for(size_t i=0;i<tree.boxes.size();++i)if(!std::isfinite(tree.boxes[i])||std::abs(tree.boxes[i])>1e6||(i%6<3&&tree.boxes[i]>tree.boxes[i+3])){napi_throw_range_error(env,nullptr,"Invalid LOD box");return Undefined(env);}
    for(size_t i=0;i<tree.lods.size();++i)if((i%3==0&&(tree.lods[i]<-1||tree.lods[i]>=1024))||(i%3!=0&&(tree.lods[i]<0||uint32_t(tree.lods[i])>splat::MaxGaussians))){napi_throw_range_error(env,nullptr,"Invalid LOD tree range");return Undefined(env);}
    tree.levels=uint32_t(levels);std::copy_n(bounds.begin(),4,tree.bounds.begin());lodTree=std::move(tree);return Undefined(env);
}
napi_value SelectLods(napi_env env,napi_callback_info info){
    napi_value args[4];size_t argc=4;napi_get_cb_info(env,info,&argc,args,nullptr,nullptr);
    std::vector<double> camera;double budget=0,fov=0,aspect=0;
    if(argc!=4||!ReadBuffer(env,args[0],camera,7)||camera.size()!=7||napi_get_value_double(env,args[1],&budget)!=napi_ok||!std::isfinite(budget)||budget<1||budget>splat::MaxDrawGaussians||napi_get_value_double(env,args[2],&fov)!=napi_ok||!std::isfinite(fov)||fov<=1||fov>=179||napi_get_value_double(env,args[3],&aspect)!=napi_ok||!std::isfinite(aspect)||aspect<=0||aspect>100){napi_throw_range_error(env,nullptr,"Invalid LOD camera");return Undefined(env);}
    for(auto v:camera)if(!std::isfinite(v)||std::abs(v)>1e4){napi_throw_range_error(env,nullptr,"Invalid LOD camera value");return Undefined(env);}
    std::array<double,7> pose;std::copy_n(camera.begin(),7,pose.begin());const auto selected=lodTree.Select(pose,uint32_t(budget),fov,aspect);
    napi_value result;void *data;const size_t bytes=selected.size()*sizeof(uint32_t);napi_create_arraybuffer(env,bytes,&data,&result);if(bytes)std::memcpy(data,selected.data(),bytes);return result;
}
napi_value ChunksImpl(napi_env env,napi_callback_info info,bool paged) {
    napi_value args[5];size_t argc=paged?5:3;napi_get_cb_info(env,info,&argc,args,nullptr,nullptr);
    bool array=false;uint32_t n=0;
    if(((paged&&argc!=4&&argc!=5)||(!paged&&argc!=2&&argc!=3))||napi_is_array(env,args[0],&array)!=napi_ok||!array||napi_get_array_length(env,args[0],&n)!=napi_ok||n>1024) {
        napi_throw_type_error(env,nullptr,"Expected 0-1024 chunk paths");return Undefined(env);
    }
    std::vector<std::string> paths;
    for(uint32_t i=0;i<n;++i) {
        napi_value v;napi_get_element(env,args[0],i,&v);size_t len=0;
        if(napi_get_value_string_utf8(env,v,nullptr,0,&len)!=napi_ok||len==0||len>4096){napi_throw_type_error(env,nullptr,"Invalid chunk path");return Undefined(env);}
        std::vector<char> p(len+1);napi_get_value_string_utf8(env,v,p.data(),p.size(),&len);
        if(std::string(p.data()).size()!=len){napi_throw_type_error(env,nullptr,"Invalid chunk path");return Undefined(env);}
        paths.emplace_back(p.data(),len);
    }
    uint32_t boundCount=0;
    if(napi_is_array(env,args[1],&array)!=napi_ok||!array||napi_get_array_length(env,args[1],&boundCount)!=napi_ok||boundCount!=4){napi_throw_type_error(env,nullptr,"Expected center and radius");return Undefined(env);}
    std::array<float,4> bounds{};
    for(int i=0;i<4;++i){napi_value v;napi_get_element(env,args[1],i,&v);double d=0;
        if(napi_get_value_double(env,v,&d)!=napi_ok||!std::isfinite(d)||std::abs(d)>1e6||(i==3&&d<=0)){napi_throw_range_error(env,nullptr,"Invalid scene bounds");return Undefined(env);}bounds[i]=float(d);}
    std::vector<uint32_t> ranges;
    bool typed=false;
    if(paged)napi_is_arraybuffer(env,args[2],&typed);
    if(typed){
        size_t bytes=0;void *data=nullptr;
        if(napi_get_arraybuffer_info(env,args[2],&data,&bytes)!=napi_ok||bytes>60000*sizeof(uint32_t)||bytes%(3*sizeof(uint32_t))){napi_throw_type_error(env,nullptr,"Invalid LOD range buffer");return Undefined(env);}
        const auto *values=static_cast<const uint32_t*>(data);const size_t length=bytes/sizeof(uint32_t);
        // Copy before any other N-API call; the engine owns the input buffer.
        if(length)ranges.assign(values,values+length);
        for(size_t i=0;i<length;++i)if(ranges[i]>splat::MaxGaussians||(i%3==0&&ranges[i]>=n)){napi_throw_range_error(env,nullptr,"Invalid LOD range buffer value");return Undefined(env);}
    } else if(argc>=3){uint32_t size=0;
        if(napi_is_array(env,args[2],&array)!=napi_ok||!array||napi_get_array_length(env,args[2],&size)!=napi_ok||size>60000||size%3){napi_throw_type_error(env,nullptr,"Invalid LOD ranges");return Undefined(env);}
        for(uint32_t i=0;i<size;++i){napi_value v;napi_get_element(env,args[2],i,&v);double d;
            if(napi_get_value_double(env,v,&d)!=napi_ok||!std::isfinite(d)||d<0||d>4000000||d!=std::floor(d)||(i%3==0&&d>=n)){napi_throw_range_error(env,nullptr,"Invalid LOD range");return Undefined(env);}ranges.push_back(uint32_t(d));}
    }
    double revision=0;
    if(paged&&(napi_get_value_double(env,args[3],&revision)!=napi_ok||!std::isfinite(revision)||revision<1||revision>9007199254740991.0||revision!=std::floor(revision))){napi_throw_range_error(env,nullptr,"Invalid selection revision");return Undefined(env);}
    bool encoded=false;if(paged&&argc==5&&napi_get_value_bool(env,args[4],&encoded)!=napi_ok){napi_throw_type_error(env,nullptr,"Expected encoded mode boolean");return Undefined(env);}
    splat::Renderer::Get().SetChunks(std::move(paths),bounds,std::move(ranges),paged,uint64_t(revision),encoded);return Undefined(env);
}
napi_value Chunks(napi_env env,napi_callback_info info){return ChunksImpl(env,info,false);}
napi_value SelectPages(napi_env env,napi_callback_info info){return ChunksImpl(env,info,true);}
napi_value TraceFrames(napi_env env,napi_callback_info info){
    napi_value arg;size_t argc=1;bool enabled=false;napi_get_cb_info(env,info,&argc,&arg,nullptr,nullptr);
    if(argc!=1||napi_get_value_bool(env,arg,&enabled)!=napi_ok){napi_throw_type_error(env,nullptr,"Expected boolean");return Undefined(env);}
    splat::Renderer::Get().TraceFrames(enabled);return Undefined(env);
}
napi_value DropCaches(napi_env env,napi_callback_info){splat::Renderer::Get().DropCaches();return Undefined(env);}
napi_value Active(napi_env env,napi_callback_info info) {
    napi_value arg;size_t argc=1;napi_get_cb_info(env,info,&argc,&arg,nullptr,nullptr);bool active;
    if(argc!=1||napi_get_value_bool(env,arg,&active)!=napi_ok){napi_throw_type_error(env,nullptr,"Expected boolean");return Undefined(env);}
    splat::Renderer::Get().SetActive(active);return Undefined(env);
}
void String(napi_env env,napi_value object,const char *key,const std::string &value){napi_value v;napi_create_string_utf8(env,value.c_str(),value.size(),&v);napi_set_named_property(env,object,key,v);}
void Number(napi_env env,napi_value object,const char *key,double value){napi_value v;napi_create_double(env,value,&v);napi_set_named_property(env,object,key,v);}
napi_value Status(napi_env env,napi_callback_info) {
    const auto s=splat::Renderer::Get().GetStatus();napi_value result;napi_create_object(env,&result);
    napi_value bounds; napi_create_array_with_length(env,4,&bounds); for(uint32_t i=0;i<4;i++){napi_value v;napi_create_double(env,s.bounds[i],&v);napi_set_element(env,bounds,i,v);} napi_set_named_property(env,result,"bounds",bounds);
    String(env,result,"state",s.state);String(env,result,"message",s.message);String(env,result,"graphics",s.graphics);
    Number(env,result,"width",s.width);Number(env,result,"height",s.height);Number(env,result,"frames",s.frames);Number(env,result,"count",s.count);Number(env,result,"bytes",s.bytes);Number(env,result,"loadMs",s.loadMs);Number(env,result,"sortMs",s.sortMs);Number(env,result,"gpuMs",s.gpuMs);Number(env,result,"uploadMs",s.uploadMs);Number(env,result,"uploadedRows",s.uploadedRows);Number(env,result,"reusedRows",s.reusedRows);Number(env,result,"decodedFiles",s.decodedFiles);Number(env,result,"subsetHits",s.subsetHits);Number(env,result,"frameMs",s.frameMs);Number(env,result,"fps",s.fps);Number(env,result,"requestRevision",s.requestRevision);Number(env,result,"displayRevision",s.displayRevision);Number(env,result,"prepareMs",s.prepareMs);Number(env,result,"refineMs",s.refineMs);Number(env,result,"uploadedBytes",s.uploadedBytes);Number(env,result,"pageHits",s.pageHits);return result;
}
struct PickWork { napi_async_work work; napi_deferred deferred; float x,y;std::vector<float> point;std::string error; };
struct InspectWork { napi_async_work work; napi_deferred deferred; std::string path,error;std::array<float,4> bounds{}; };
napi_value InspectModel(napi_env env,napi_callback_info info) {
    napi_value arg;size_t argc=1,length=0;napi_get_cb_info(env,info,&argc,&arg,nullptr,nullptr);
    if(argc!=1||napi_get_value_string_utf8(env,arg,nullptr,0,&length)!=napi_ok||!length||length>4096){napi_throw_type_error(env,nullptr,"Invalid model path");return Undefined(env);}
    std::vector<char> path(length+1);napi_get_value_string_utf8(env,arg,path.data(),path.size(),&length);
    if(std::strlen(path.data())!=length){napi_throw_type_error(env,nullptr,"Invalid model path");return Undefined(env);}
    auto *job=new InspectWork{};job->path.assign(path.data(),length);napi_value promise,name;
    napi_create_promise(env,&job->deferred,&promise);napi_create_string_utf8(env,"InspectGaussianBounds",NAPI_AUTO_LENGTH,&name);
    napi_create_async_work(env,nullptr,name,[](napi_env,void *p){auto *j=static_cast<InspectWork*>(p);
        try {j->bounds=splat::InspectModel(j->path);}
        catch(const std::exception &e){j->error=e.what();}},
        [](napi_env e,napi_status status,void *p){auto *j=static_cast<InspectWork*>(p);napi_value result;
            if(status!=napi_ok||!j->error.empty()){napi_value text;napi_create_string_utf8(e,j->error.empty()?"Model inspection cancelled":j->error.c_str(),NAPI_AUTO_LENGTH,&text);napi_create_error(e,nullptr,text,&result);napi_reject_deferred(e,j->deferred,result);}
            else{napi_create_array_with_length(e,4,&result);for(uint32_t i=0;i<4;i++){napi_value v;napi_create_double(e,j->bounds[i],&v);napi_set_element(e,result,i,v);}napi_resolve_deferred(e,j->deferred,result);}
            napi_delete_async_work(e,j->work);delete j;},job,&job->work);
    napi_queue_async_work(env,job->work);return promise;
}
napi_value Pick(napi_env env,napi_callback_info info) {
    napi_value args[2];size_t argc=2;napi_get_cb_info(env,info,&argc,args,nullptr,nullptr);double x,y;
    if(argc!=2||napi_get_value_double(env,args[0],&x)!=napi_ok||napi_get_value_double(env,args[1],&y)!=napi_ok||
        !std::isfinite(x)||!std::isfinite(y)||x<0||x>1||y<0||y>1){napi_throw_range_error(env,nullptr,"Invalid pick coordinates");return Undefined(env);}
    auto *job=new PickWork{};job->x=x;job->y=y;napi_value promise,name;napi_create_promise(env,&job->deferred,&promise);
    napi_create_string_utf8(env,"GaussianPick",NAPI_AUTO_LENGTH,&name);
    napi_create_async_work(env,nullptr,name,[](napi_env,void *p){auto *j=static_cast<PickWork*>(p);try{j->point=splat::Renderer::Get().Pick(j->x,j->y);}catch(const std::exception &e){j->error=e.what();}},
        [](napi_env e,napi_status status,void *p){auto *j=static_cast<PickWork*>(p);napi_value result;
            if(status!=napi_ok||!j->error.empty()){napi_value text;napi_create_string_utf8(e,"Picking failed",NAPI_AUTO_LENGTH,&text);napi_create_error(e,nullptr,text,&result);napi_reject_deferred(e,j->deferred,result);}
            else{napi_create_array_with_length(e,j->point.size(),&result);for(size_t i=0;i<j->point.size();++i){napi_value v;napi_create_double(e,j->point[i],&v);napi_set_element(e,result,i,v);}napi_resolve_deferred(e,j->deferred,result);}
            napi_delete_async_work(e,j->work);delete j;},job,&job->work);
    napi_queue_async_work(env,job->work);return promise;
}
napi_value Init(napi_env env,napi_value exports) {
    RegisterCollision(env,exports);
    napi_property_descriptor methods[]={
        {"inspectModel",nullptr,InspectModel,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"registerLods",nullptr,RegisterLods,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"selectLods",nullptr,SelectLods,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"traceFrames",nullptr,TraceFrames,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"dropCaches",nullptr,DropCaches,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"selectPages",nullptr,SelectPages,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"pick",nullptr,Pick,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"load",nullptr,Load,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"chunks",nullptr,Chunks,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"optimize",nullptr,Optimize,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"camera",nullptr,Camera,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"setActive",nullptr,Active,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"status",nullptr,Status,nullptr,nullptr,nullptr,napi_default,nullptr}
    };
    napi_define_properties(env,exports,sizeof(methods)/sizeof(methods[0]),methods);
    napi_value value;bool has=false;napi_has_named_property(env,exports,OH_NATIVE_XCOMPONENT_OBJ,&has);
    if(has&&napi_get_named_property(env,exports,OH_NATIVE_XCOMPONENT_OBJ,&value)==napi_ok){
        OH_NativeXComponent *component=nullptr;
        if(napi_unwrap(env,value,reinterpret_cast<void**>(&component))==napi_ok&&component)OH_NativeXComponent_RegisterCallback(component,&callbacks);
    }
    return exports;
}
}
static napi_module module={1,0,nullptr,Init,"splat",nullptr,{0}};
extern "C" __attribute__((constructor)) void RegisterSplat(){napi_module_register(&module);}
