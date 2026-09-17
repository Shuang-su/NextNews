#include <qos/qos.h>
#include <hilog/log.h>
#include "renderer.h"
#include "post_shaders.h"
#include "sh_shader.h"
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
/*TONE_FUNCTIONS*/
uniform int directTone;
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
    uvec4 data=texelFetch(encodedCodes,address,0);uint row=data.w&2047u;
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
uniform highp usampler2D shColorCache;
uniform bool shColorCached;
/*SH_FUNCTIONS*/
uniform mat4 view;
uniform vec2 viewport;
uniform float nearPlane;
uniform float tanHalfFov;
uniform float farPlane;
uniform bool optimized;
uniform float introProgress;
uniform vec4 introBounds;
uniform float introTime;
uniform vec4 introMotion;
uniform float introOscillation;
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
    vec3 sourcePosition=position;
    // MetaFlow dot wave followed by the delayed lift wave. Covariance trace
    // gives the same RMS size as gsplatGetSizeFromScale without eigenvectors.
    if (introProgress < 1.0) {
        float dist=length(position-introBounds.xyz);
        float dotWave=introMotion.x*introTime+0.5*introMotion.y*introTime*introTime;
        float liftTime=max(0.0,introTime-introMotion.z);
        float liftWave=introMotion.x*liftTime+0.5*introMotion.y*liftTime*liftTime;
        if(dist>introBounds.w)color.a=0.0;
        if(dist<=introBounds.w && (liftTime<=0.0 || dist>liftWave-1.5)){
            float phase=fract(sin(dot(position,vec3(127.1,311.7,74.7)))*43758.5453)*6.28318;
            position.y+=sin(introTime*3.0+phase)*introOscillation*0.25;
        }
        bool lifted=liftTime>0.0 && liftWave>dist;
        float revealScale=0.035;
        if(lifted)revealScale=mix(0.035,1.0,clamp((liftWave-dist)*0.5,0.0,1.0));
        else if(dist>dotWave+0.005)color.a=0.0;
        else if(dist>max(dotWave-1.0,0.0)) {
            float waveDistance=abs(dist-dotWave);
            revealScale=waveDistance<0.5?mix(0.035,0.07,1.0-waveDistance*2.0):
                0.035*(1.0-smoothstep(max(dotWave-1.0,0.0),dotWave+0.005,dist));
        }
        if(revealScale<1.0) {
            float originalSize=sqrt(max(0.0,(covariance[0][0]+covariance[1][1]+covariance[2][2])/3.0));
            float dotSize=introMotion.w;
            float size=lifted?mix(dotSize,originalSize*revealScale,(revealScale-0.035)/0.965):dotSize*revealScale/0.035;
            size=min(size,originalSize);covariance=mat3(size*size);
        }
    }
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
    if(shColorCached)color.rgb=uintBitsToFloat(texelFetch(shColorCache,ivec2(int(splatIndex%4096u),int(splatIndex/4096u)),0).rgb);
    else if(shBands>0)color.rgb=directionalColor(sourcePosition,view,color.rgb);
    if(directTone>1){
        vec3 linear=pow(max(color.rgb,vec3(0)),vec3(2.2));
        if(directTone==2)linear=toneMap2(linear);else if(directTone==3)linear=toneMap3(linear);
        else if(directTone==4)linear=toneMap4(linear);else if(directTone==5)linear=toneMap5(linear);else if(directTone==6)linear=toneMap6(linear);
        color.rgb=pow(max(linear,vec3(0))+0.0000001,vec3(1.0/2.2));
    }
    tint=color;
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
uint64_t Renderer::BeginSkybox(){std::lock_guard<std::mutex> lock(mutex_);skyImage_.reset();skyFailed_=false;status_.skyError.clear();status_.skyReady=false;dirty_=true;changed_.notify_one();return ++skyRequest_;}
bool Renderer::SkyboxCurrent(uint64_t request){std::lock_guard<std::mutex> lock(mutex_);return request==skyRequest_;}
bool Renderer::SetSkybox(uint64_t request,std::shared_ptr<const SkyImage> image){std::lock_guard<std::mutex> lock(mutex_);if(request!=skyRequest_)return false;skyImage_=std::move(image);skyFailed_=false;dirty_=true;changed_.notify_one();return true;}
// Keep the existing bridge ABI, but higher-order shading is disabled by product policy.
void Renderer::SetShDegree(int){std::lock_guard<std::mutex> lock(mutex_);shDegree_=0;dirty_=true;changed_.notify_one();}
void Renderer::SetEffects(Effects settings){std::lock_guard<std::mutex> lock(mutex_);if(effects_!=settings){effects_=settings;effectsFailed_=false;status_.postError.clear();dirty_=true;changed_.notify_one();}}
Renderer &Renderer::Get() { static Renderer renderer; return renderer; }
Renderer::~Renderer() { Stop(); }
void Renderer::Start(void *window,int width,int height) {
    Stop();
    { std::lock_guard<std::mutex> lock(mutex_); stop_=false; cancel_=false; dirty_=true; width_=std::max(1,width); height_=std::max(1,height);
      if(status_.openingRequest>0 && status_.openingPresented==status_.openingRequest && scene_->Count())openingCommitted_=true;
      if(!chunkPaths_.empty())chunksDirty_=true;
      if(status_.state=="loading"&&pendingPath_.empty())pendingPath_=requestedPath_;
    }
    loader_=std::thread(&Renderer::LoadLoop,this);
    sorter_=std::thread(&Renderer::SortLoop,this);
    worker_=std::thread(&Renderer::Loop,this,window);
}
void Renderer::Stop() {
    { std::lock_guard<std::mutex> lock(mutex_); stop_=true; ++surfaceGeneration_; cancel_=true; intro_.Pause(); }
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
        }catch(const std::exception &e){std::lock_guard<std::mutex> lock(mutex_);status_.state="error";status_.errorRequest=status_.openingRequest;status_.message=e.what();}
    }
}
void Renderer::Resize(int width,int height) { {std::lock_guard<std::mutex> lock(mutex_);width_=std::max(1,width);height_=std::max(1,height);dirty_=true;}changed_.notify_one(); }
void Renderer::Load(std::string path) {
    {std::lock_guard<std::mutex> lock(mutex_);++loadGeneration_;loadOpeningRequest_=status_.openingRequest;preparedScene_.reset();preparedPage_.reset();chunksPaged_=false;chunkPaths_.clear();chunksDirty_=false;requestedPath_=path;pendingPath_=std::move(path);cancel_=true;status_.state="loading";status_.message="Loading model";}
    loadChanged_.notify_one();
}
void Renderer::SetChunks(std::vector<std::string> paths, std::array<float,4> bounds, std::vector<uint32_t> ranges,bool paged,uint64_t revision,bool encoded) {
    {std::lock_guard<std::mutex> lock(mutex_);++loadGeneration_;loadOpeningRequest_=status_.openingRequest;preparedScene_.reset();preparedPage_.reset();chunksPaged_=paged;chunksEncoded_=encoded;requestRevision_=revision;requestAt_=std::chrono::duration<double,std::milli>(Clock::now().time_since_epoch()).count();status_.requestRevision=revision;chunkPaths_=std::move(paths);chunkBounds_=bounds;chunkRanges_=std::move(ranges);
     pendingPath_.clear();requestedPath_.clear();chunksDirty_=!chunkPaths_.empty();cancel_=true;dirty_=true;
     status_.state=chunkPaths_.empty()?"ready":"loading";status_.message=chunkPaths_.empty()?"Stream stopped":"Streaming chunks";}
    loadChanged_.notify_one();changed_.notify_one();
}
void Renderer::TraceFrames(bool enabled){std::lock_guard<std::mutex> lock(mutex_);traceFrames_=enabled;lastTraceFrame_=0;}
void Renderer::DropCaches(){std::lock_guard<std::mutex> lock(mutex_);dropCaches_=true;}
void Renderer::SetCamera(Camera camera) {
    {std::lock_guard<std::mutex> lock(mutex_);camera_=camera;dirty_=true;}changed_.notify_one();
}
void Renderer::SetActive(bool active) { {std::lock_guard<std::mutex> lock(mutex_);active_=active;intro_.Pause();dirty_=true;}changed_.notify_one();loadChanged_.notify_one(); }
void Renderer::SetIntro(bool enabled,bool waitForModel) {
    {std::lock_guard<std::mutex> lock(mutex_);intro_.Request(enabled,waitForModel,scene_->radius);if(waitForModel){++status_.openingRequest;openingCommitted_=false;openingMinGeneration_=loadGeneration_+1;}dirty_=true;}
    changed_.notify_one();
}
bool Renderer::IsPresented(uint64_t request,uint64_t surface) {
    std::lock_guard<std::mutex> lock(mutex_);
    return !stop_ && surface==surfaceGeneration_ && request==status_.openingRequest && request==status_.openingPresented;
}
void Renderer::OnPresented(std::function<void(uint64_t,uint64_t)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);presentedCallback_=std::move(callback);
}
void Renderer::ExtendOpening() {
    if(!scene_->hasWorldBox)return;
    const float radius=FarthestCorner({introBounds_[0],introBounds_[1],introBounds_[2]},scene_->worldBox);
    if(radius>introBounds_[3]){introBounds_[3]=radius;intro_.Extend(radius);}
}
bool Renderer::BeginIntro(uint64_t request, std::array<float,3> focus, const std::vector<float>& box, int profile) {
    {std::lock_guard<std::mutex> lock(mutex_);
        if(stop_||presentedSurface_!=surfaceGeneration_||request!=status_.openingRequest||request!=status_.openingPresented)return false;
        std::array<float,6> bounds=scene_->worldBox;
        if(box.size()==6){
            for(int k=0;k<3;++k){bounds[k]=scene_->hasWorldBox?std::min(box[k],bounds[k]):box[k];bounds[k+3]=scene_->hasWorldBox?std::max(box[k+3],bounds[k+3]):box[k+3];}
        }
        else if(!scene_->hasWorldBox)for(int k=0;k<3;++k){bounds[k]=scene_->center[k]-scene_->radius;bounds[k+3]=scene_->center[k]+scene_->radius;}
        introBounds_={focus[0],focus[1],focus[2],FarthestCorner(focus,bounds)};
        intro_.BeginVisible(introBounds_[3],profile);dirty_=true;
        OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsOpening","VisiblePlayback request=%{public}llu profile=%{public}d radius=%{public}.3f",(unsigned long long)request,profile,introBounds_[3]);
    }
    changed_.notify_one();return true;
}

void Renderer::SetBackground(std::array<float,3> color) {
    { std::lock_guard<std::mutex> lock(mutex_); if(background_==color)return; background_=color;dirty_=true; }
    changed_.notify_one();
}
void Renderer::SetAnnotations(std::shared_ptr<const HotspotData> data){{std::lock_guard<std::mutex> lock(mutex_);annotationData_=std::move(data);dirty_=true;}changed_.notify_one();}
void Renderer::SetAnnotationStyle(HotspotStyle style){{std::lock_guard<std::mutex> lock(mutex_);if(style.visible==annotationStyle_.visible&&style.hover==annotationStyle_.hover&&style.pixels==annotationStyle_.pixels)return;annotationStyle_=style;dirty_=true;}changed_.notify_one();}
std::vector<float> Renderer::Pick(float x,float y) {
    std::shared_ptr<Scene> scene;Camera camera;int width,height;
    {std::lock_guard<std::mutex> lock(mutex_);scene=scene_;camera=camera_;width=width_;height=height_;}
    if(!scene) return {};
    return splat::Pick(*scene,MakeView(*scene,camera),x,y,width,height);
}
Status Renderer::GetStatus() {std::lock_guard<std::mutex> lock(mutex_);status_.width=width_;status_.height=height_;if(scene_){status_.bounds={scene_->center[0],scene_->center[1],scene_->center[2],scene_->radius};}auto result=status_;if(stop_||presentedSurface_!=surfaceGeneration_)result.openingPresented=0;return result;}
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
    auto makeProgram=[&](bool cached){
        std::string source=Vertex;const std::string toneMarker="/*TONE_FUNCTIONS*/",shMarker="/*SH_FUNCTIONS*/";
        source.replace(source.find(toneMarker),toneMarker.size(),PostTone);
        source.replace(source.find(shMarker),shMarker.size(),cached?
            "uniform int shBands; vec3 directionalColor(vec3 p,mat4 v,vec3 c){return c;}":ShShader);
        GLuint vertex=Compile(GL_VERTEX_SHADER,source.c_str()),fragment=0;
        try{fragment=Compile(GL_FRAGMENT_SHADER,Fragment);}catch(...){glDeleteShader(vertex);throw;}
        GLuint program=glCreateProgram();glAttachShader(program,vertex);glAttachShader(program,fragment);glLinkProgram(program);glDeleteShader(vertex);glDeleteShader(fragment);
        GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){glDeleteProgram(program);throw std::runtime_error("Gaussian shader link failed");}return program;
    };
    fullProgram_=makeProgram(false);cachedProgram_=makeProgram(true);program_=0;
    glGenVertexArrays(1,&vao_);glBindVertexArray(vao_);glGenBuffers(1,&buffer_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
    glEnableVertexAttribArray(0);glVertexAttribIPointer(0,1,GL_UNSIGNED_INT,sizeof(uint32_t),nullptr);glVertexAttribDivisor(0,1);
    GLint maxTexture=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxTexture);atlasRows_=std::min(maxTexture,8192);atlas_.Reset(atlasRows_*4);encodedRows_=std::min(maxTexture,4096);encodedAtlas_.Reset(encodedRows_*16);encodedBookSlots_.assign(encodedRows_*16,UINT32_MAX);bookAtlas_.Reset(2048);shAtlas_.Reset(ShAtlasPages);
    if(maxTexture<4096)throw std::runtime_error("4096-wide data textures unavailable");
    glGenTextures(1,&dataTexture_);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,dataTexture_);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
}
void Renderer::DestroyGL() {
    if(shCentroidAtlas_)glDeleteTextures(1,&shCentroidAtlas_);if(shMapping_)glDeleteTextures(1,&shMapping_);
    shCentroidAtlas_=shMapping_=0;shAtlas_.Reset(0);activeShPages_.clear();shMappingRows_.clear();
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
        shWorkbuffer_.Destroy();shTexture_.Destroy();stagingShTexture_.Destroy();sky_.Destroy();post_.Destroy();hotspots_.Destroy();depthBits_=0;
        if(timerQueries_[0])glDeleteQueries(4,timerQueries_);
        if(dataTexture_)glDeleteTextures(1,&dataTexture_);
        if(buffer_)glDeleteBuffers(1,&buffer_);if(vao_)glDeleteVertexArrays(1,&vao_);if(fullProgram_)glDeleteProgram(fullProgram_);if(cachedProgram_)glDeleteProgram(cachedProgram_);fullProgram_=cachedProgram_=program_=0;
    }
    timerResult_=nullptr;timerSlot_=0;
    std::fill_n(timerQueries_,4,0);std::fill_n(timerPending_,4,false);std::fill_n(timerInvalid_,4,false);
    {std::lock_guard<std::mutex> lock(mutex_);status_.gpuMs=-1;status_.annotationDepth=0;status_.postBytes=0;status_.postActive=0;effectsFailed_=false;status_.skyBytes=0;status_.skyReady=false;skyFailed_=false;}
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
        stagingShTexture_.Destroy();
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
    const bool shReady=stagingRow_==rows&&stagingShTexture_.Prepare(stagingScene_,(64-uploaded)*65536);
    if(stagingRow_==rows&&shReady){
        std::lock_guard<std::mutex> lock(mutex_);
        if(stagingGeneration_==loadGeneration_){
            shTexture_.Swap(stagingShTexture_);stagingShTexture_.Destroy();activeShPages_.clear();activePages_.clear();retiredScenes_.push_back(std::move(scene_));scene_=std::move(stagingScene_);ExtendOpening();if(scene_->Count() && stagingGeneration_>=openingMinGeneration_){intro_.Commit(scene_->radius);openingCommitted_=status_.openingPresented<status_.openingRequest;}initialIndices_=std::move(stagingIndices_);
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
    if(!shTexture_.Prepare(scene_,4*1024*1024)){std::lock_guard<std::mutex> lock(mutex_);dirty_=true;return;}
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
    std::shared_ptr<const HotspotData> annotations;HotspotStyle style;std::array<float,3> background;std::array<float,4> introBounds;float introProgress;float introTime;RevealMotion introMotion;uint64_t openingRequest,openingSurface;
    {std::lock_guard<std::mutex> lock(mutex_);annotations=annotationData_;style=annotationStyle_;background=background_;introBounds=introBounds_;openingRequest=status_.openingRequest;openingSurface=surfaceGeneration_;introProgress=intro_.Frame(std::chrono::duration<double>(Clock::now().time_since_epoch()).count());introTime=float(intro_.Seconds());introMotion=intro_.Motion();}
    if(introProgress<1)style.visible=false;
    const bool annotationReady=depthBits_>0&&hotspots_.Prepare(annotations);
    {std::lock_guard<std::mutex> lock(mutex_);status_.annotationDepth=annotationReady?depthBits_:0;}
    int shDegree;{std::lock_guard<std::mutex> lock(mutex_);shDegree=shDegree_;}
    const bool shCached=(!uploadDirty_||preuploaded_)&&shWorkbuffer_.Prepare(scene_,view,shDegree,dataTexture_,shTexture_);
    Effects effects;{std::lock_guard<std::mutex> lock(mutex_);effects=effects_;}
    bool failed;{std::lock_guard<std::mutex> lock(mutex_);failed=effectsFailed_;}
    bool post=false;
    try {post=post_.Begin(width,height,failed?Effects{}:effects);}
    catch(const std::exception& error){
        post_.Destroy();glBindFramebuffer(GL_FRAMEBUFFER,0);while(glGetError()!=GL_NO_ERROR){}
        std::lock_guard<std::mutex> lock(mutex_);effectsFailed_=true;status_.postError=error.what();
        OH_LOG_Print(LOG_APP,LOG_ERROR,0xD003,"NextNewsPost","%{public}s",error.what());
    }
    glViewport(0,0,width,height);glClearColor(background[0],background[1],background[2],1);glClear(GL_COLOR_BUFFER_BIT);
    std::shared_ptr<const SkyImage> skyImage;bool skyFailed;{std::lock_guard<std::mutex> lock(mutex_);skyImage=skyImage_;skyFailed=skyFailed_;}
    try {sky_.Prepare(skyFailed?nullptr:skyImage);sky_.Draw(view,width,height,post?0:int(effects[0]));}
    catch(const std::exception& error){sky_.Destroy();glActiveTexture(GL_TEXTURE0);while(glGetError()!=GL_NO_ERROR){}std::lock_guard<std::mutex> lock(mutex_);skyFailed_=true;status_.skyError=error.what();}
    {std::lock_guard<std::mutex> lock(mutex_);status_.skyBytes=sky_.Bytes();status_.skyReady=skyImage_&&skyImage_==skyImage&&!skyFailed_&&!sky_.Pending();if(sky_.Pending())dirty_=true;}
    if(annotationReady&&style.visible){glDepthMask(GL_TRUE);glClearDepthf(1);glClear(GL_DEPTH_BUFFER_BIT);hotspots_.Draw(view,width,height,style,false,post?0:int(effects[0]));}
    if(annotationReady&&style.visible){glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);}else glDisable(GL_DEPTH_TEST);
    const GLuint selectedProgram=(shCached||shDegree==0||scene_->shDegree==0)?cachedProgram_:fullProgram_;
    if(program_!=selectedProgram){
        program_=selectedProgram;
        viewLocation_=glGetUniformLocation(program_,"view");viewportLocation_=glGetUniformLocation(program_,"viewport");nearLocation_=glGetUniformLocation(program_,"nearPlane");farLocation_=glGetUniformLocation(program_,"farPlane");dataLocation_=glGetUniformLocation(program_,"splatData");optimizedLocation_=glGetUniformLocation(program_,"optimized");
    }
    glDepthMask(GL_FALSE);glUseProgram(program_);
    shWorkbuffer_.Bind(program_,shCached);
    // A paged SH0 scene must not sample a previous single-model SH texture.
    shTexture_.Bind(program_,scene_->paged?0:shDegree);
    const bool pagedSh=scene_->paged&&encodedDrawable_&&scene_->shDegree>0;
    glUniform1i(glGetUniformLocation(program_,"shPaged"),pagedSh);
    if(pagedSh){
        glUniform1i(glGetUniformLocation(program_,"shBands"),shDegree);glUniform1i(glGetUniformLocation(program_,"shFlip"),scene_->shTransform);
        glActiveTexture(GL_TEXTURE6);glBindTexture(GL_TEXTURE_2D,shMapping_);glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,shCentroidAtlas_);glActiveTexture(GL_TEXTURE0);
    }
    glUniform1i(glGetUniformLocation(program_,"directTone"),post?0:int(effects[0]));
    glUniform1i(optimizedLocation_,optimized_.load());
    glUniform1f(glGetUniformLocation(program_,"introProgress"),introProgress);
    glUniform1f(glGetUniformLocation(program_,"introTime"),introTime);
    glUniform4f(glGetUniformLocation(program_,"introMotion"),introMotion.speed,introMotion.acceleration,introMotion.delay,introMotion.dotSize);
    glUniform1f(glGetUniformLocation(program_,"introOscillation"),introMotion.oscillation);
    glUniform4f(glGetUniformLocation(program_,"introBounds"),introBounds[0],introBounds[1],introBounds[2],introBounds[3]);
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
                std::lock_guard<std::mutex> lock(mutex_);status_.gpuMs=timerInvalid_[i]||ns==0?-1:double(ns)/1e6;}
        }
        if(!timerPending_[timerSlot_]){timerInvalid_[timerSlot_]=false;glBeginQuery(0x88BF,timerQueries_[timerSlot_]);timed=true;}
    }
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,GLsizei(scene_->paged && !atlasDrawable_ ? 0 : scene_->Count()));
    if(annotationReady&&style.visible)hotspots_.Draw(view,width,height,style,true,post?0:int(effects[0]));
    if(post)post_.Finish();
    if(timed){glEndQuery(0x88BF);timerPending_[timerSlot_]=true;timerSlot_=(timerSlot_+1)%4;}
    {std::lock_guard<std::mutex> lock(mutex_);status_.postBytes=post_.Bytes();status_.postActive=post?1:0;}
    const GLenum error=glGetError();if(error!=GL_NO_ERROR)throw std::runtime_error("OpenGL error: "+std::to_string(error));
    if(!eglSwapBuffers(display_,surface_))throw std::runtime_error("EGL swap failed");
    std::lock_guard<std::mutex> lock(mutex_);
    if(!stop_ && openingCommitted_ && openingSurface==surfaceGeneration_ && openingRequest==status_.openingRequest && scene_->Count()){status_.openingPresented=openingRequest;presentedSurface_=openingSurface;openingCommitted_=false;if(presentedCallback_)presentedCallback_(openingRequest,openingSurface);OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsOpening","FirstFrame request=%{public}llu",(unsigned long long)openingRequest);}
    if(pendingDisplayRevision_ && scene_->paged && atlasDrawable_){
        status_.displayRevision=pendingDisplayRevision_;status_.refineMs=std::chrono::duration<double,std::milli>(Clock::now().time_since_epoch()).count()-pendingDisplayAt_;
        OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","PageDisplay revision=%{public}llu count=%{public}zu prepareMs=%{public}.2f sortMs=%{public}.2f refineMs=%{public}.2f uploadedBytes=%{public}.0f pageHits=%{public}.0f frameMs=%{public}.2f",(unsigned long long)pendingDisplayRevision_,scene_->Count(),pendingPrepareMs_,pendingSortMs_,status_.refineMs,status_.uploadedBytes,status_.pageHits,Ms(drawStart));
        pendingDisplayRevision_=0;
    }
    if(traceFrames_){const double now=std::chrono::duration<double,std::milli>(Clock::now().time_since_epoch()).count();
        if(lastTraceFrame_>0)OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","StreamPresent intervalMs=%{public}.3f submitMs=%{public}.3f count=%{public}zu revision=%{public}.0f",now-lastTraceFrame_,Ms(drawStart),scene_->Count(),status_.displayRevision);
        lastTraceFrame_=now;}
    status_.shSource=scene_->shDegree;status_.shActive=std::min(scene_->shDegree,shDegree);status_.shBytes=shWorkbuffer_.Bytes()+(shCentroidAtlas_?size_t(4096)*4096*4+ShSourcePages*2048*4:0)+shTexture_.Bytes()+stagingShTexture_.Bytes()+scene_->harmonics.capacity()*sizeof(std::array<float,48>)+scene_->shLabels.capacity()*sizeof(std::array<uint32_t,2>)+(scene_->sogHarmonics?scene_->sogHarmonics->Bytes():0);
    status_.frames++;status_.sortMs=sortMs;status_.frameMs=Ms(drawStart);status_.fps=1000.0/std::max(Ms(start),.001);status_.bytes=status_.shBytes+(preparedPixels_.capacity()+stagingPixels_.capacity())*sizeof(float)+cacheBytes_.load()+scene_->points.capacity()*sizeof(Gaussian)+scene_->Count()*68+sorted.capacity()*sizeof(uint32_t);
}
void Renderer::LoadLoop() {
    const int qos=OH_QoS_SetThreadQoS(QOS_USER_INITIATED);
    OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","Thread QoS LoadLoop() result=%{public}d",qos);
    for (;;) {
        std::vector<std::vector<float>> retiredPixels;std::vector<std::shared_ptr<Scene>> retiredScenes;
        std::string path;std::vector<std::string> chunks;std::array<float,4> bounds{};std::vector<uint32_t> ranges;uint64_t generation,openingRequest=0,revision=0;bool paged=false,encoded=false,dropCaches=false;double requestAt=0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            loadChanged_.wait(lock,[&]{return stop_||!retiredPixels_.empty()||!retiredScenes_.empty()||(active_&&(!pendingPath_.empty()||chunksDirty_));});
            retiredPixels.swap(retiredPixels_);retiredScenes.swap(retiredScenes_);
            if(stop_)return;
            if(!active_)continue;
            generation=loadGeneration_;openingRequest=loadOpeningRequest_;paged=chunksPaged_;encoded=chunksEncoded_;revision=requestRevision_;requestAt=requestAt_;path=std::move(pendingPath_);pendingPath_.clear();
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
                const auto &g=loaded->points[i];loaded->Include(g.position);float *p=pixels.data()+i*16;
                std::copy_n(g.position,3,p);std::copy_n(g.color,4,p+3);std::copy_n(g.covariance,6,p+7);
            }
            loaded->FitClipping();
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
                    decoded_.Clear();selected_.Clear();auto loaded=ReadModel(path,&cancel_,false,false);
                    const uint32_t count=loaded.points.size();
                    publish(std::make_shared<Scene>(std::move(loaded)),false,Ms(start),{{nextSourceId_++,0,count}});
                } catch(const std::exception &e) {
                    std::lock_guard<std::mutex> lock(mutex_);if(!cancel_ && generation==loadGeneration_ && openingRequest==status_.openingRequest){status_.state="error";status_.errorRequest=openingRequest;status_.message=e.what();}
                }
            }
            if(!chunks.empty() && paged){
                try{PreparePages(chunks,bounds,ranges,generation,revision,requestAt,encoded);}
                catch(const std::exception &e){std::lock_guard<std::mutex> lock(mutex_);if(!cancel_ && generation==loadGeneration_ && openingRequest==status_.openingRequest){status_.state="error";status_.errorRequest=openingRequest;status_.message=e.what();}}
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
                            if(!part){part=std::make_shared<Scene>(ReadModel(chunks[file],&cancel_,false,false));decoded_.Put(chunks[file],part);++decodedFiles;}
                            subset=std::make_shared<Scene>();
                            for(size_t i=0;i<selection.size();i+=2){
                                const size_t offset=selection[i],count=selection[i+1]?selection[i+1]:part->points.size();
                                if(offset>part->points.size()||count>part->points.size()-offset||subset->points.size()+count>MaxGaussians)throw std::runtime_error("LOD selection exceeds model or resident budget");
                                AppendRange(*subset,*part,offset,count,&cancel_);
                            }
                            selected_.Put(selectionKey,subset);
                        }
                        if(combined.points.size()+subset->points.size()>MaxGaussians)throw std::runtime_error("Resident budget exceeded");
                        AppendRange(combined,*subset,0,subset->Count(),&cancel_);
                        size_t explicitCount=0,wholeCopies=0;
                        for(size_t i=1;i<selection.size();i+=2){if(selection[i])explicitCount+=selection[i];else ++wholeCopies;}
                        const uint32_t wholeCount=wholeCopies?uint32_t((subset->points.size()-explicitCount)/wholeCopies):0;
                        for(size_t i=0;i<selection.size();i+=2)spans.push_back({source,selection[i],selection[i+1]?selection[i+1]:wholeCount});
                    }
                    {std::lock_guard<std::mutex> lock(mutex_);status_.decodedFiles=decodedFiles;status_.subsetHits=subsetHits;}
                    publish(std::make_shared<Scene>(std::move(combined)),false,Ms(start),spans);
                } catch(const std::exception &e) {
                    std::lock_guard<std::mutex> lock(mutex_);if(!cancel_ && generation==loadGeneration_ && openingRequest==status_.openingRequest){status_.state="error";status_.errorRequest=openingRequest;status_.message=e.what();}
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
                if(active_ && intro_.Running()) {
                    changed_.wait_for(lock,std::chrono::milliseconds(16),[&]{return stop_||!active_||dirty_||preparedScene_||preparedPage_;});
                } else {
                    changed_.wait(lock,[&]{return stop_||(active_&&(dirty_||preparedScene_||preparedPage_||intro_.Running()));});
                }
                if(stop_)break;
                if(!active_)continue;
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
    }catch(const std::exception &e){std::lock_guard<std::mutex> lock(mutex_);status_.state="error";status_.errorRequest=status_.openingRequest;status_.message=e.what();}
    DestroyGL();
}
}
