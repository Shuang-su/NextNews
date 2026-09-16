#include <qos/qos.h>
#include <hilog/log.h>
#include "renderer.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <native_window/external_window.h>

namespace splat {
namespace {
using Clock = std::chrono::steady_clock;
double Ms(Clock::time_point t) { return std::chrono::duration<double, std::milli>(Clock::now()-t).count(); }
const char *Vertex = R"GLSL(#version 300 es
precision highp float;
layout(location=0) in uint splatIndex;
uniform highp sampler2D splatData;
uniform highp usampler2D encodedCodes;
uniform highp sampler2D codebooks;
uniform bool encoded;
vec4 bytes(uint v){return vec4(v&255u,(v>>8u)&255u,(v>>16u)&255u,v>>24u);}
float book(uint code,uint row,int component){return texelFetch(codebooks,ivec2(int(code&255u),int(row)),0)[component];}
void readEncoded(out vec3 position,out vec4 color,out mat3 covariance){
    ivec2 address=ivec2(int(splatIndex%4096u),int(splatIndex/4096u));
    position=texelFetch(splatData,address,0).xyz;
    uvec4 data=texelFetch(encodedCodes,address,0);uint row=data.w;
    color=vec4(vec3(book(data.z,row,1),book(data.z>>8u,row,1),book(data.z>>16u,row,1)),float(data.z>>24u)/255.0);
    vec3 abc=(bytes(data.x).xyz/255.0-.5)*1.41421356237;
    float d=sqrt(max(0.0,1.0-dot(abc,abc)));vec4 q;
    uint mode=data.x>>24u;
    if(mode==252u)q=vec4(d,abc);else if(mode==253u)q=vec4(abc.x,d,abc.yz);else if(mode==254u)q=vec4(abc.xy,d,abc.z);else q=vec4(abc,d);
    q=normalize(q);float w=q.x,x=q.y,y=q.z,z=q.w;
    mat3 r=mat3(1.0-2.0*(y*y+z*z),2.0*(x*y+z*w),2.0*(x*z-y*w),
                2.0*(x*y-z*w),1.0-2.0*(x*x+z*z),2.0*(y*z+x*w),
                2.0*(x*z+y*w),2.0*(y*z-x*w),1.0-2.0*(x*x+y*y));
    vec3 scale=vec3(book(data.y,row,0),book(data.y>>8u,row,0),book(data.y>>16u,row,0));
    covariance=(r*mat3(scale.x,0,0,0,scale.y,0,0,0,scale.z))*transpose(r);
    covariance[0][2]=-covariance[0][2];covariance[2][0]=-covariance[2][0];
    covariance[1][2]=-covariance[1][2];covariance[2][1]=-covariance[2][1];
}
vec4 readSplat(uint offset) { uint address=splatIndex*4u+offset;return texelFetch(splatData,ivec2(int(address%4096u),int(address/4096u)),0); }
uniform mat4 view;
uniform vec2 viewport;
uniform float nearPlane;
uniform float tanHalfFov;
uniform float farPlane;
uniform bool optimized;
out vec2 gaussian;
out vec4 tint;
void main() {
    vec3 position;vec4 color;mat3 covariance;
    if(encoded){readEncoded(position,color,covariance);}
    else {vec4 t0=readSplat(0u),t1=readSplat(1u),t2=readSplat(2u),t3=readSplat(3u);
        position=t0.xyz;color=vec4(t0.w,t1.xyz);
        vec3 covA=vec3(t1.w,t2.xy),covB=vec3(t2.zw,t3.x);
        covariance=mat3(covA.x,covA.y,covA.z,covA.y,covB.x,covB.y,covA.z,covB.y,covB.z);
    }
    tint=color;
    vec3 center=(view*vec4(position,1.0)).xyz;
    float z=-center.z;
    if(z<=nearPlane || z>=farPlane || color.a<0.0039) {
        gl_Position=vec4(2.0,2.0,2.0,1.0); gaussian=vec2(0.0); return;
    }
    float focal=viewport.y/(2.0*tanHalfFov);
    mat3 rotation=mat3(view);
    mat3 c=rotation*covariance*transpose(rotation);
    // Perspective projection derivative, screen coordinates in pixels.
    vec3 jx=vec3(focal/z,0.0,focal*clamp(center.x/z,-(1.3*tanHalfFov)*viewport.x/viewport.y,(1.3*tanHalfFov)*viewport.x/viewport.y)/z);
    vec3 jy=vec3(0.0,focal/z,focal*clamp(center.y/z,-(1.3*tanHalfFov),(1.3*tanHalfFov))/z);
    float a=dot(jx,c*jx)+0.3;
    float b=dot(jx,c*jy);
    float d=dot(jy,c*jy)+0.3;
    // Bound extreme input projections before the eigenvalue calculation.
    float covarianceLimit=max(max(a,d),abs(b))/1e8;
    if(covarianceLimit>1.0){a/=covarianceLimit;b/=covarianceLimit;d/=covarianceLimit;}
    float mid=(a+d)*0.5;
    float delta=length(vec2((a-d)*0.5,b));
    float l1=max(mid+delta,0.3), l2=max(mid-delta,0.3);
    vec2 axis=abs(b)>0.000001?normalize(vec2(b,l1-a)):(a>=d?vec2(1,0):vec2(0,1));
    vec2 corners[4]=vec2[4](vec2(-1,-1),vec2(1,-1),vec2(-1,1),vec2(1,1));
    // The fragment cutoff is 1/255. Tightening support to this contour
    // removes only fragments which the original shader would discard.
    float support=optimized?min(3.0,sqrt(max(0.0,2.0*log(255.0*color.a)))):3.0;
    // Match the reference viewer's screen-space major/minor radius ceiling.
    // The old 4096-sigma ceiling allowed a single sky splat to cover 12k pixels.
    float sigmaLimit=optimized?min(1024.0,min(viewport.x,viewport.y))/3.0:4096.0;
    vec2 major=axis*min(sqrt(l1),sigmaLimit);
    vec2 minor=vec2(-axis.y,axis.x)*min(sqrt(l2),sigmaLimit);
    vec2 projected=focal*center.xy/z;
    vec2 extent=(abs(major)+abs(minor))*support;
    if(optimized && any(greaterThan(abs(projected)-extent,viewport*0.5))) {
        gl_Position=vec4(0.0,0.0,2.0,1.0);gaussian=vec2(0.0);return;
    }
    gaussian=corners[gl_VertexID]*support;
    vec2 offset=major*gaussian.x+minor*gaussian.y;
    vec2 ndc=(projected+offset)*2.0/viewport;
    float depth=(farPlane+nearPlane)/(farPlane-nearPlane)-2.0*farPlane*nearPlane/((farPlane-nearPlane)*z);
    gl_Position=vec4(ndc,depth,1.0);
})GLSL";
const char *Fragment=R"GLSL(#version 300 es
precision highp float;
in vec2 gaussian;
in vec4 tint;
out vec4 outColor;
void main() {
    float power=dot(gaussian,gaussian);
    if(power>9.0) discard;
    float alpha=min(0.99,tint.a*exp(-0.5*power));
    if(alpha<1.0/255.0) discard;
    outColor=vec4(tint.rgb*alpha,alpha);
})GLSL";
GLuint Compile(GLenum type,const char *source) {
    GLuint shader=glCreateShader(type); glShaderSource(shader,1,&source,nullptr); glCompileShader(shader);
    GLint ok; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[4096]{}; glGetShaderInfoLog(shader,sizeof(log),nullptr,log); glDeleteShader(shader); throw std::runtime_error(std::string("Shader: ")+log); }
    return shader;
}
}
Renderer &Renderer::Get() { static Renderer renderer; return renderer; }
Renderer::~Renderer() { Stop(); }
void Renderer::Start(void *window,int width,int height) {
    Stop();
    { std::lock_guard<std::mutex> lock(mutex_); stop_=false; cancel_=false; dirty_=true; width_=std::max(1,width); height_=std::max(1,height);
      if(!chunkPaths_.empty())chunksDirty_=true;
      if(status_.state=="loading"&&pendingPath_.empty())pendingPath_=requestedPath_;
    }
    loader_=std::thread(&Renderer::LoadLoop,this);
    sorter_=std::thread(&Renderer::SortLoop,this);
    worker_=std::thread(&Renderer::Loop,this,window);
}
void Renderer::Stop() {
    { std::lock_guard<std::mutex> lock(mutex_); stop_=true; cancel_=true; }
    changed_.notify_all();sortChanged_.notify_all();loadChanged_.notify_all(); if(worker_.joinable()) worker_.join();if(sorter_.joinable())sorter_.join();if(loader_.joinable())loader_.join();
    preparedScene_.reset();preparedPage_.reset();preparedPixels_.clear();preparedIndices_.clear();
    sortPending_=sortReady_=false;sortScene_.reset();sortedScene_.reset();sortedIndices_.clear();
}
void Renderer::SortLoop() {
    const int qos=OH_QoS_SetThreadQoS(QOS_USER_INITIATED);
    OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","Thread QoS SortLoop() result=%{public}d",qos);
    for(;;) {
        std::shared_ptr<Scene> scene;View view;
        {std::unique_lock<std::mutex> lock(mutex_);sortChanged_.wait(lock,[&]{return stop_||sortPending_;});
         if(stop_)return;scene=sortScene_;view=sortView_;sortPending_=false;}
        const auto start=Clock::now();
        try {
            auto indices=SortIndices(*scene,view);const double elapsed=Ms(start);
            {std::lock_guard<std::mutex> lock(mutex_);if(stop_)return;
             if(scene_!=scene)continue;sortedIndices_=std::move(indices);sortedScene_=scene;sortReady_=true;completedSortMs_=elapsed;dirty_=true;}
            changed_.notify_one();
        }catch(const std::exception &e){std::lock_guard<std::mutex> lock(mutex_);status_.state="error";status_.message=e.what();}
    }
}
void Renderer::Resize(int width,int height) { {std::lock_guard<std::mutex> lock(mutex_);width_=std::max(1,width);height_=std::max(1,height);dirty_=true;}changed_.notify_one(); }
void Renderer::Load(std::string path) {
    {std::lock_guard<std::mutex> lock(mutex_);++loadGeneration_;preparedScene_.reset();preparedPage_.reset();chunksPaged_=false;chunkPaths_.clear();chunksDirty_=false;requestedPath_=path;pendingPath_=std::move(path);cancel_=true;status_.state="loading";status_.message="Loading model";}
    loadChanged_.notify_one();
}
void Renderer::SetChunks(std::vector<std::string> paths, std::array<float,4> bounds, std::vector<uint32_t> ranges,bool paged,uint64_t revision,bool encoded) {
    {std::lock_guard<std::mutex> lock(mutex_);++loadGeneration_;preparedScene_.reset();preparedPage_.reset();chunksPaged_=paged;chunksEncoded_=encoded;requestRevision_=revision;requestAt_=std::chrono::duration<double,std::milli>(Clock::now().time_since_epoch()).count();status_.requestRevision=revision;chunkPaths_=std::move(paths);chunkBounds_=bounds;chunkRanges_=std::move(ranges);
     pendingPath_.clear();requestedPath_.clear();chunksDirty_=!chunkPaths_.empty();cancel_=true;dirty_=true;
     status_.state=chunkPaths_.empty()?"ready":"loading";status_.message=chunkPaths_.empty()?"Stream stopped":"Streaming chunks";}
    loadChanged_.notify_one();changed_.notify_one();
}
void Renderer::TraceFrames(bool enabled){std::lock_guard<std::mutex> lock(mutex_);traceFrames_=enabled;lastTraceFrame_=0;}
void Renderer::DropCaches(){std::lock_guard<std::mutex> lock(mutex_);dropCaches_=true;}
void Renderer::SetCamera(Camera camera) {
    {std::lock_guard<std::mutex> lock(mutex_);camera_=camera;dirty_=true;}changed_.notify_one();
}
void Renderer::SetActive(bool active) { {std::lock_guard<std::mutex> lock(mutex_);active_=active;dirty_=true;}changed_.notify_one();loadChanged_.notify_one(); }
void Renderer::SetAnnotations(std::shared_ptr<const HotspotData> data){{std::lock_guard<std::mutex> lock(mutex_);annotationData_=std::move(data);dirty_=true;}changed_.notify_one();}
void Renderer::SetAnnotationStyle(HotspotStyle style){{std::lock_guard<std::mutex> lock(mutex_);if(style.visible==annotationStyle_.visible&&style.hover==annotationStyle_.hover&&style.pixels==annotationStyle_.pixels)return;annotationStyle_=style;dirty_=true;}changed_.notify_one();}
std::vector<float> Renderer::Pick(float x,float y) {
    std::shared_ptr<Scene> scene;Camera camera;int width,height;
    {std::lock_guard<std::mutex> lock(mutex_);scene=scene_;camera=camera_;width=width_;height=height_;}
    if(!scene) return {};
    return splat::Pick(*scene,MakeView(*scene,camera),x,y,width,height);
}
Status Renderer::GetStatus() {std::lock_guard<std::mutex> lock(mutex_);status_.width=width_;status_.height=height_;if(scene_){status_.bounds={scene_->center[0],scene_->center[1],scene_->center[2],scene_->radius};}return status_;}
void Renderer::InitGL(void *window) {
    dataCapacity_=0;uploadDirty_=true; window_ = window; bufferWidth_ = bufferHeight_ = 0;
    display_=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display_==EGL_NO_DISPLAY || !eglInitialize(display_,nullptr,nullptr)) throw std::runtime_error("EGL display initialization failed");
    EGLint configAttrs[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    EGLConfig config; EGLint count=0;
    for(int depth:{24,16,0}){configAttrs[13]=depth;if(eglChooseConfig(display_,configAttrs,&config,1,&count)&&count)break;}
    if(!count) throw std::runtime_error("OpenGL ES 3 window configuration unavailable");
    const EGLint attrs[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    context_=eglCreateContext(display_,config,EGL_NO_CONTEXT,attrs);
    surface_=eglCreateWindowSurface(display_,config,reinterpret_cast<EGLNativeWindowType>(window),nullptr);
    if(context_==EGL_NO_CONTEXT||surface_==EGL_NO_SURFACE||!eglMakeCurrent(display_,surface_,surface_,context_)) throw std::runtime_error("EGL surface/context creation failed");
    eglSwapInterval(display_,1);
    glGetIntegerv(GL_DEPTH_BITS,&depthBits_);
    const auto *version=glGetString(GL_VERSION), *renderer=glGetString(GL_RENDERER);
    {std::lock_guard<std::mutex> lock(mutex_);status_.graphics=std::string(version?reinterpret_cast<const char*>(version):"unknown")+" / "+(renderer?reinterpret_cast<const char*>(renderer):"unknown");}
    // Optional, asynchronous hardware timing. Never wait for query completion.
    const char *extensions=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if(extensions && std::string(extensions).find("GL_EXT_disjoint_timer_query")!=std::string::npos) {
        timerResult_=reinterpret_cast<TimerResult>(eglGetProcAddress("glGetQueryObjectui64vEXT"));
        if(timerResult_)glGenQueries(4,timerQueries_);
    }
    GLuint vertex=Compile(GL_VERTEX_SHADER,Vertex), fragment=0;
    try {fragment=Compile(GL_FRAGMENT_SHADER,Fragment);} catch(...) {glDeleteShader(vertex);throw;}
    program_=glCreateProgram();glAttachShader(program_,vertex);glAttachShader(program_,fragment);glLinkProgram(program_);glDeleteShader(vertex);glDeleteShader(fragment);
    GLint ok;glGetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok) throw std::runtime_error("Gaussian shader link failed");
    viewLocation_=glGetUniformLocation(program_,"view");viewportLocation_=glGetUniformLocation(program_,"viewport");nearLocation_=glGetUniformLocation(program_,"nearPlane");farLocation_=glGetUniformLocation(program_,"farPlane");dataLocation_=glGetUniformLocation(program_,"splatData");optimizedLocation_=glGetUniformLocation(program_,"optimized");
    glGenVertexArrays(1,&vao_);glBindVertexArray(vao_);glGenBuffers(1,&buffer_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
    glEnableVertexAttribArray(0);glVertexAttribIPointer(0,1,GL_UNSIGNED_INT,sizeof(uint32_t),nullptr);glVertexAttribDivisor(0,1);
    GLint maxTexture=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxTexture);atlasRows_=std::min(maxTexture,8192);atlas_.Reset(atlasRows_*4);encodedRows_=std::min(maxTexture,4096);encodedAtlas_.Reset(encodedRows_*16);encodedBookSlots_.assign(encodedRows_*16,UINT32_MAX);bookAtlas_.Reset(2048);
    if(maxTexture<4096)throw std::runtime_error("4096-wide data textures unavailable");
    glGenTextures(1,&dataTexture_);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,dataTexture_);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
}
void Renderer::DestroyGL() {
    if(encodedCenters_)glDeleteTextures(1,&encodedCenters_);if(encodedCodes_)glDeleteTextures(1,&encodedCodes_);if(codebookTexture_)glDeleteTextures(1,&codebookTexture_);
    encodedCenters_=encodedCodes_=codebookTexture_=0;encodedDrawable_=false;encodedBookSlots_.clear();encodedAtlas_.Reset(0);bookAtlas_.Reset(0);activeEncodedPages_.clear();activeBooks_.clear();
    if(atlasTexture_)glDeleteTextures(1,&atlasTexture_);atlasTexture_=0;atlasDrawable_=false;atlas_.Reset(0);activePages_.clear();stagingPage_.reset();
    if(stagingTexture_)glDeleteTextures(1,&stagingTexture_);stagingTexture_=0;
    if(spareTexture_)glDeleteTextures(1,&spareTexture_);spareTexture_=0;spareCapacity_=dataCapacity_=0;
    stagingScene_.reset();stagingPixels_.clear();stagingIndices_.clear();preuploaded_=false;
    dataRows_.clear();spareRows_.clear();stagingRows_.clear();stagingPreviousRows_.clear();
    if(display_==EGL_NO_DISPLAY)return;
    if(context_!=EGL_NO_CONTEXT && surface_!=EGL_NO_SURFACE){
        eglMakeCurrent(display_,surface_,surface_,context_);
        hotspots_.Destroy();depthBits_=0;
        if(timerQueries_[0])glDeleteQueries(4,timerQueries_);
        if(dataTexture_)glDeleteTextures(1,&dataTexture_);
        if(buffer_)glDeleteBuffers(1,&buffer_);if(vao_)glDeleteVertexArrays(1,&vao_);if(program_)glDeleteProgram(program_);
    }
    timerResult_=nullptr;timerSlot_=0;
    std::fill_n(timerQueries_,4,0);std::fill_n(timerPending_,4,false);std::fill_n(timerInvalid_,4,false);
    {std::lock_guard<std::mutex> lock(mutex_);status_.gpuMs=-1;status_.annotationDepth=0;}
    dataTexture_=buffer_=vao_=program_=0;eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    if(surface_!=EGL_NO_SURFACE)eglDestroySurface(display_,surface_);
    if(context_!=EGL_NO_CONTEXT)eglDestroyContext(display_,context_);
    eglTerminate(display_);display_=EGL_NO_DISPLAY;surface_=EGL_NO_SURFACE;context_=EGL_NO_CONTEXT;
}
void Renderer::SetOptimized(bool enabled) {
    optimized_=enabled;{std::lock_guard<std::mutex> lock(mutex_);dirty_=true;}changed_.notify_one();
}
void Renderer::AdvanceUpload() {
    if(!stagingScene_)return;
    bool stale;{std::lock_guard<std::mutex> lock(mutex_);stale=stagingGeneration_!=loadGeneration_;}
    if(stale){
        if(stagingTexture_)glDeleteTextures(1,&stagingTexture_);stagingTexture_=0;
        stagingScene_.reset();stagingPixels_.clear();stagingIndices_.clear();
        std::lock_guard<std::mutex> lock(mutex_);dirty_=true;return;
    }
    const auto uploadStart=Clock::now();
    const size_t rows=stagingPixels_.size()/(4096*4);
    glActiveTexture(GL_TEXTURE0);
    if(!stagingTexture_){
        if(spareTexture_ && spareCapacity_>=rows){
            stagingTexture_=spareTexture_;stagingCapacity_=spareCapacity_;spareTexture_=0;spareCapacity_=0;
            glBindTexture(GL_TEXTURE_2D,stagingTexture_);
            stagingPreviousRows_=std::move(spareRows_);
        }else{
            if(spareTexture_)glDeleteTextures(1,&spareTexture_);spareTexture_=0;spareCapacity_=0;
            spareRows_.clear();stagingPreviousRows_.clear();
            stagingCapacity_=1;while(stagingCapacity_<rows)stagingCapacity_*=2;
            glGenTextures(1,&stagingTexture_);glBindTexture(GL_TEXTURE_2D,stagingTexture_);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,4096,stagingCapacity_);
        }
    }else glBindTexture(GL_TEXTURE_2D,stagingTexture_);
    // At most 4 MiB per frame. The old scene/texture remain drawable while this fills.
    size_t uploaded=0;
    while(stagingRow_<rows && uploaded<64){
        if(SameUploadRow(stagingRows_,stagingPreviousRows_,stagingRow_)){
            ++stagingRow_;++stagingReusedRows_;continue;
        }
        const size_t begin=stagingRow_;
        while(stagingRow_<rows && uploaded<64 && !SameUploadRow(stagingRows_,stagingPreviousRows_,stagingRow_)){
            ++stagingRow_;++uploaded;
        }
        glTexSubImage2D(GL_TEXTURE_2D,0,0,begin,4096,stagingRow_-begin,GL_RGBA,GL_FLOAT,stagingPixels_.data()+begin*4096*4);
    }
    stagingUploadedRows_+=uploaded;
    if(stagingRow_==rows){
        std::lock_guard<std::mutex> lock(mutex_);
        if(stagingGeneration_==loadGeneration_){
            activePages_.clear();retiredScenes_.push_back(std::move(scene_));scene_=std::move(stagingScene_);initialIndices_=std::move(stagingIndices_);
            spareTexture_=dataTexture_;spareCapacity_=dataCapacity_;
            spareRows_=std::move(dataRows_);dataRows_=std::move(stagingRows_);stagingPreviousRows_.clear();
            status_.uploadedRows=stagingUploadedRows_;status_.reusedRows=stagingReusedRows_;
            dataTexture_=stagingTexture_;dataCapacity_=stagingCapacity_;stagingTexture_=0;
            uploadDirty_=true;preuploaded_=true;sortPending_=sortReady_=false;
            sortScene_.reset();sortedScene_.reset();sortedIndices_.clear();
            if(stagingResetCamera_)camera_=Camera{};
            sortScene_=scene_;sortView_=MakeView(*scene_,camera_);sortPending_=true;sortChanged_.notify_one();
            status_.count=scene_->Count();status_.state="ready";status_.message="Resident set ready";
        }
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if(!stagingScene_){retiredPixels_.push_back(std::move(stagingPixels_));loadChanged_.notify_one();}
    status_.uploadMs=Ms(uploadStart);dirty_=true;
}
void Renderer::Draw(const View &view,int width,int height) {
    if(bufferWidth_!=width || bufferHeight_!=height) {
        const int result=OH_NativeWindow_NativeWindowHandleOpt(static_cast<OHNativeWindow*>(window_),SET_BUFFER_GEOMETRY,width,height);
        if(result!=0)throw std::runtime_error("Native window buffer resize failed: "+std::to_string(result));
        bufferWidth_=width;bufferHeight_=height;
        // The currently acquired EGL buffer may still have the previous size.
        // Swap it out, then render once more even if the camera is idle.
        std::lock_guard<std::mutex> lock(mutex_);dirty_=true;
    }
    const auto start=Clock::now();
    const std::array<float,3> direction={view.matrix[2],view.matrix[6],view.matrix[10]};
    bool resort=uploadDirty_;std::vector<uint32_t> sorted;double sortMs=0;
    if(uploadDirty_) {
        if(initialIndices_.size()==scene_->Count())sorted=std::move(initialIndices_);
        else sorted=SortIndices(*scene_,view);
        sortMs=Ms(start);sortDirection_=direction;
    }
    else {
        if(direction!=sortDirection_) {
            {std::lock_guard<std::mutex> lock(mutex_);sortScene_=scene_;sortView_=view;sortPending_=true;}
            sortDirection_=direction;sortChanged_.notify_one();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if(sortReady_) {
            if(sortedScene_==scene_){sorted=std::move(sortedIndices_);resort=true;sortMs=completedSortMs_;}
            sortReady_=false;sortedScene_.reset();
        }
    }
    const auto drawStart=Clock::now();
    std::shared_ptr<const HotspotData> annotations;HotspotStyle style;
    {std::lock_guard<std::mutex> lock(mutex_);annotations=annotationData_;style=annotationStyle_;}
    const bool annotationReady=depthBits_>0&&hotspots_.Prepare(annotations);
    {std::lock_guard<std::mutex> lock(mutex_);status_.annotationDepth=annotationReady?depthBits_:0;}
    glViewport(0,0,width,height);glClearColor(.035f,.045f,.065f,1);glClear(GL_COLOR_BUFFER_BIT);
    if(annotationReady&&style.visible){glDepthMask(GL_TRUE);glClearDepthf(1);glClear(GL_DEPTH_BUFFER_BIT);hotspots_.Draw(view,width,height,style,false);}
    if(annotationReady&&style.visible){glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);}else glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);glUseProgram(program_);
    glUniform1i(optimizedLocation_,optimized_.load());
    glUniformMatrix4fv(viewLocation_,1,GL_FALSE,view.matrix.data());
    glUniform2f(viewportLocation_,float(width),float(height));
    glUniform1f(glGetUniformLocation(program_,"tanHalfFov"),view.tanHalfFov);glUniform1f(nearLocation_,view.nearPlane);glUniform1f(farLocation_,view.farPlane);
    glUniform1i(glGetUniformLocation(program_,"encoded"),scene_->paged&&encodedDrawable_);
    glUniform1i(glGetUniformLocation(program_,"encodedCodes"),1);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,encodedCodes_);
    glUniform1i(glGetUniformLocation(program_,"codebooks"),2);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,codebookTexture_);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,scene_->paged?(encodedDrawable_?encodedCenters_:atlasTexture_):dataTexture_);glUniform1i(dataLocation_,0);
    if(uploadDirty_ && !preuploaded_ && !scene_->paged) {
        const size_t height=std::max(size_t(1),(scene_->Count()*4+4095)/4096);
        if(uploadPixels_.empty()) {
            uploadPixels_.resize(height*4096*4,0);
            for(size_t i=0;i<scene_->Count();++i) {const auto &g=scene_->points[i];float *p=uploadPixels_.data()+i*16;
                std::copy_n(g.position,3,p);std::copy_n(g.color,4,p+3);std::copy_n(g.covariance,6,p+7);}
        }
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,4096,height,0,GL_RGBA,GL_FLOAT,uploadPixels_.data());dataCapacity_=height;
        std::vector<float>().swap(uploadPixels_);
    }
    if(resort && scene_->paged)for(auto &index:sorted)index=scene_->addresses.at(index);
    glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);if(resort){glBufferData(GL_ARRAY_BUFFER,sorted.size()*sizeof(uint32_t),sorted.data(),GL_DYNAMIC_DRAW);uploadDirty_=false;preuploaded_=false;}
    bool timed=false;
    if(timerResult_) {
        GLint disjoint=0;glGetIntegerv(0x8FBB,&disjoint); // GPU_DISJOINT_EXT
        if(disjoint)std::fill_n(timerInvalid_,4,true);
        for(int i=0;i<4;++i)if(timerPending_[i]) {
            GLuint available=0;glGetQueryObjectuiv(timerQueries_[i],GL_QUERY_RESULT_AVAILABLE,&available);
            if(available){GLuint64 ns=0;timerResult_(timerQueries_[i],GL_QUERY_RESULT,&ns);timerPending_[i]=false;
                std::lock_guard<std::mutex> lock(mutex_);status_.gpuMs=timerInvalid_[i]?-1:double(ns)/1e6;}
        }
        if(!timerPending_[timerSlot_]){timerInvalid_[timerSlot_]=false;glBeginQuery(0x88BF,timerQueries_[timerSlot_]);timed=true;}
    }
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,GLsizei(scene_->paged && !atlasDrawable_ ? 0 : scene_->Count()));
    if(timed){glEndQuery(0x88BF);timerPending_[timerSlot_]=true;timerSlot_=(timerSlot_+1)%4;}
    if(annotationReady&&style.visible)hotspots_.Draw(view,width,height,style,true);
    const GLenum error=glGetError();if(error!=GL_NO_ERROR)throw std::runtime_error("OpenGL error: "+std::to_string(error));
    if(!eglSwapBuffers(display_,surface_))throw std::runtime_error("EGL swap failed");
    std::lock_guard<std::mutex> lock(mutex_);
    if(pendingDisplayRevision_ && scene_->paged && atlasDrawable_){
        status_.displayRevision=pendingDisplayRevision_;status_.refineMs=std::chrono::duration<double,std::milli>(Clock::now().time_since_epoch()).count()-pendingDisplayAt_;
        OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","PageDisplay revision=%{public}llu count=%{public}zu prepareMs=%{public}.2f sortMs=%{public}.2f refineMs=%{public}.2f uploadedBytes=%{public}.0f pageHits=%{public}.0f frameMs=%{public}.2f",(unsigned long long)pendingDisplayRevision_,scene_->Count(),pendingPrepareMs_,pendingSortMs_,status_.refineMs,status_.uploadedBytes,status_.pageHits,Ms(drawStart));
        pendingDisplayRevision_=0;
    }
    if(traceFrames_){const double now=std::chrono::duration<double,std::milli>(Clock::now().time_since_epoch()).count();
        if(lastTraceFrame_>0)OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","StreamPresent intervalMs=%{public}.3f submitMs=%{public}.3f count=%{public}zu revision=%{public}.0f",now-lastTraceFrame_,Ms(drawStart),scene_->Count(),status_.displayRevision);
        lastTraceFrame_=now;}
    status_.frames++;status_.sortMs=sortMs;status_.frameMs=Ms(drawStart);status_.fps=1000.0/std::max(Ms(start),.001);status_.bytes=(preparedPixels_.capacity()+stagingPixels_.capacity())*sizeof(float)+cacheBytes_.load()+scene_->points.capacity()*sizeof(Gaussian)+scene_->Count()*68+sorted.capacity()*sizeof(uint32_t);
}
void Renderer::LoadLoop() {
    const int qos=OH_QoS_SetThreadQoS(QOS_USER_INITIATED);
    OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","Thread QoS LoadLoop() result=%{public}d",qos);
    for (;;) {
        std::vector<std::vector<float>> retiredPixels;std::vector<std::shared_ptr<Scene>> retiredScenes;
        std::string path;std::vector<std::string> chunks;std::array<float,4> bounds{};std::vector<uint32_t> ranges;uint64_t generation,revision=0;bool paged=false,encoded=false,dropCaches=false;double requestAt=0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            loadChanged_.wait(lock,[&]{return stop_||!retiredPixels_.empty()||!retiredScenes_.empty()||(active_&&(!pendingPath_.empty()||chunksDirty_));});
            retiredPixels.swap(retiredPixels_);retiredScenes.swap(retiredScenes_);
            if(stop_)return;
            if(!active_)continue;
            generation=loadGeneration_;paged=chunksPaged_;encoded=chunksEncoded_;revision=requestRevision_;requestAt=requestAt_;path=std::move(pendingPath_);pendingPath_.clear();
            if(chunksDirty_){chunks=chunkPaths_;bounds=chunkBounds_;ranges=chunkRanges_;chunksDirty_=false;}
            if(!path.empty()||!chunks.empty()){dropCaches=dropCaches_;dropCaches_=false;}cancel_=false;
        }
        retiredPixels.clear();retiredScenes.clear();
        if(path.empty()&&chunks.empty())continue;
        if(dropCaches){decoded_.Clear();selected_.Clear();pageCache_.Clear();sourceIds_.clear();}
        // Prepare CPU uploads and the first sorted index buffer without occupying EGL.
        auto publish=[&](std::shared_ptr<Scene> loaded,bool reset,double loadMs,const std::vector<SourceSpan> &spans) {
            Camera camera;{std::lock_guard<std::mutex> lock(mutex_);camera=reset?Camera{}:camera_;}
            const View view=MakeView(*loaded,camera);
            auto indices=SortIndices(*loaded,view);
            const size_t rows=std::max(size_t(1),(loaded->points.size()*4+4095)/4096);
            std::vector<float> pixels(rows*4096*4,0);
            for(size_t i=0;i<loaded->points.size();++i){
                if((i&4095)==0&&cancel_)return;
                const auto &g=loaded->points[i];float *p=pixels.data()+i*16;
                std::copy_n(g.position,3,p);std::copy_n(g.color,4,p+3);std::copy_n(g.covariance,6,p+7);
            }
            auto rowIdentities=MakeUploadRows(spans);
            std::lock_guard<std::mutex> lock(mutex_);
            if(cancel_||generation!=loadGeneration_)return;
            preparedRows_=std::move(rowIdentities);
            preparedScene_=std::move(loaded);preparedPixels_=std::move(pixels);preparedIndices_=std::move(indices);
            preparedResetCamera_=reset;dirty_=true;status_.loadMs=loadMs;
            status_.message="CPU resident set prepared";
        };
            if(!path.empty()) {
                const auto start=Clock::now();
                try {
                    decoded_.Clear();selected_.Clear();auto loaded=ReadModel(path,&cancel_);
                    const uint32_t count=loaded.points.size();
                    publish(std::make_shared<Scene>(std::move(loaded)),true,Ms(start),{{nextSourceId_++,0,count}});
                } catch(const std::exception &e) {
                    std::lock_guard<std::mutex> lock(mutex_);if(!cancel_){status_.state="error";status_.message=e.what();}
                }
            }
            if(!chunks.empty() && paged){
                try{PreparePages(chunks,bounds,ranges,generation,revision,requestAt,encoded);}
                catch(const std::exception &e){std::lock_guard<std::mutex> lock(mutex_);if(!cancel_ && generation==loadGeneration_){status_.state="error";status_.message=e.what();}}
            }
            if(!chunks.empty() && !paged) {
                const auto start=Clock::now();
                try {
                    size_t decodedFiles=0,subsetHits=0;
                    Scene combined;std::vector<SourceSpan> spans;size_t requested=0;for(size_t i=2;i<ranges.size();i+=3)requested+=ranges[i];if(requested>MaxGaussians)throw std::runtime_error("Resident budget exceeded");combined.points.reserve(requested);combined.center={bounds[0],bounds[1],bounds[2]};combined.radius=bounds[3];
                    if(ranges.empty())for(size_t i=0;i<chunks.size();++i){ranges.push_back(i);ranges.push_back(0);ranges.push_back(0);}
                    for(size_t file=0;file<chunks.size();++file) {
                        if(cancel_)throw std::runtime_error("Load cancelled");
                        std::vector<uint32_t> selection;
                        for(size_t i=0;i<ranges.size();i+=3)if(ranges[i]==file){selection.push_back(ranges[i+1]);selection.push_back(ranges[i+2]);}
                        auto &source=sourceIds_[chunks[file]];if(!source)source=nextSourceId_++;
                        const auto selectionKey=std::make_pair(chunks[file],selection);
                        auto subset=selected_.Get(selectionKey);
                        if(subset)++subsetHits;
                        else {
                            auto part=decoded_.Get(chunks[file]);
                            if(!part){part=std::make_shared<Scene>(ReadModel(chunks[file],&cancel_));decoded_.Put(chunks[file],part);++decodedFiles;}
                            subset=std::make_shared<Scene>();
                            for(size_t i=0;i<selection.size();i+=2){
                                const size_t offset=selection[i],count=selection[i+1]?selection[i+1]:part->points.size();
                                if(offset>part->points.size()||count>part->points.size()-offset||subset->points.size()+count>MaxGaussians)throw std::runtime_error("LOD selection exceeds model or resident budget");
                                subset->points.insert(subset->points.end(),part->points.begin()+offset,part->points.begin()+offset+count);
                            }
                            selected_.Put(selectionKey,subset);
                        }
                        if(combined.points.size()+subset->points.size()>MaxGaussians)throw std::runtime_error("Resident budget exceeded");
                        combined.points.insert(combined.points.end(),subset->points.begin(),subset->points.end());
                        size_t explicitCount=0,wholeCopies=0;
                        for(size_t i=1;i<selection.size();i+=2){if(selection[i])explicitCount+=selection[i];else ++wholeCopies;}
                        const uint32_t wholeCount=wholeCopies?uint32_t((subset->points.size()-explicitCount)/wholeCopies):0;
                        for(size_t i=0;i<selection.size();i+=2)spans.push_back({source,selection[i],selection[i+1]?selection[i+1]:wholeCount});
                    }
                    {std::lock_guard<std::mutex> lock(mutex_);status_.decodedFiles=decodedFiles;status_.subsetHits=subsetHits;}
                    publish(std::make_shared<Scene>(std::move(combined)),false,Ms(start),spans);
                } catch(const std::exception &e) {
                    std::lock_guard<std::mutex> lock(mutex_);if(!cancel_){status_.state="error";status_.message=e.what();}
                }
            }

        cacheBytes_=decoded_.Bytes()+selected_.Bytes()+pageCache_.Bytes();changed_.notify_one();
    }
}
void Renderer::Loop(void *window) {
    const int qos=OH_QoS_SetThreadQoS(QOS_USER_INTERACTIVE);
    OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","Thread QoS Loop(void *window) result=%{public}d",qos);
    try {
        InitGL(window);
        for(;;) {
            Camera camera;int width,height;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                changed_.wait(lock,[&]{return stop_||(active_&&(dirty_||preparedScene_||preparedPage_));});
                if(stop_)break;
                if(preparedScene_ && !stagingScene_){
                    stagingScene_=std::move(preparedScene_);stagingPixels_=std::move(preparedPixels_);
                    stagingIndices_=std::move(preparedIndices_);stagingResetCamera_=preparedResetCamera_;
                    stagingGeneration_=loadGeneration_;stagingRow_=0;
                    stagingRows_=std::move(preparedRows_);stagingPreviousRows_.clear();
                    stagingUploadedRows_=stagingReusedRows_=0;
                }
                if(preparedPage_ && !stagingPage_)stagingPage_=std::move(preparedPage_);
                camera=camera_;width=width_;height=height_;dirty_=false;
            }
            AdvanceUpload();
            AdvancePages();
            {std::lock_guard<std::mutex> lock(mutex_);camera=camera_;}
            Draw(MakeView(*scene_,camera),width,height);
            {std::lock_guard<std::mutex> lock(mutex_);if(status_.state=="waiting"){status_.state="ready";status_.message="OpenGL ES surface ready";}}
        }
    }catch(const std::exception &e){std::lock_guard<std::mutex> lock(mutex_);status_.state="error";status_.message=e.what();}
    DestroyGL();
}
}
