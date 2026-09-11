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
    napi_value args[6];size_t argc=6;napi_get_cb_info(env,info,&argc,args,nullptr,nullptr);double v[6];
    if(argc!=6){napi_throw_type_error(env,nullptr,"Expected six camera values");return Undefined(env);}
    for(int i=0;i<6;++i)if(napi_get_value_double(env,args[i],&v[i])!=napi_ok||!std::isfinite(v[i])||std::abs(v[i])>1e4){napi_throw_range_error(env,nullptr,"Invalid camera value");return Undefined(env);}
    splat::Renderer::Get().SetCamera({float(v[0]),float(v[1]),float(v[2]),float(v[3]),float(v[4]),float(v[5])});return Undefined(env);
}
napi_value Active(napi_env env,napi_callback_info info) {
    napi_value arg;size_t argc=1;napi_get_cb_info(env,info,&argc,&arg,nullptr,nullptr);bool active;
    if(argc!=1||napi_get_value_bool(env,arg,&active)!=napi_ok){napi_throw_type_error(env,nullptr,"Expected boolean");return Undefined(env);}
    splat::Renderer::Get().SetActive(active);return Undefined(env);
}
void String(napi_env env,napi_value object,const char *key,const std::string &value){napi_value v;napi_create_string_utf8(env,value.c_str(),value.size(),&v);napi_set_named_property(env,object,key,v);}
void Number(napi_env env,napi_value object,const char *key,double value){napi_value v;napi_create_double(env,value,&v);napi_set_named_property(env,object,key,v);}
napi_value Status(napi_env env,napi_callback_info) {
    const auto s=splat::Renderer::Get().GetStatus();napi_value result;napi_create_object(env,&result);
    String(env,result,"state",s.state);String(env,result,"message",s.message);String(env,result,"graphics",s.graphics);
    Number(env,result,"count",s.count);Number(env,result,"bytes",s.bytes);Number(env,result,"loadMs",s.loadMs);Number(env,result,"sortMs",s.sortMs);Number(env,result,"frameMs",s.frameMs);Number(env,result,"fps",s.fps);return result;
}
napi_value Init(napi_env env,napi_value exports) {
    napi_property_descriptor methods[]={
        {"load",nullptr,Load,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"camera",nullptr,Camera,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"setActive",nullptr,Active,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"status",nullptr,Status,nullptr,nullptr,nullptr,napi_default,nullptr}
    };
    napi_define_properties(env,exports,4,methods);
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
