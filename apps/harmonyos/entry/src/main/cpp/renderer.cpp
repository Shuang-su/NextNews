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
vec4 readSplat(uint offset) { uint address=splatIndex*4u+offset;return texelFetch(splatData,ivec2(int(address%4096u),int(address/4096u)),0); }
uniform mat4 view;
uniform vec2 viewport;
uniform float nearPlane;
uniform float farPlane;
out vec2 gaussian;
out vec4 tint;
void main() {
    vec4 t0=readSplat(0u),t1=readSplat(1u),t2=readSplat(2u),t3=readSplat(3u);
    vec3 position=t0.xyz;vec4 color=vec4(t0.w,t1.xyz);
    vec3 covA=vec3(t1.w,t2.xy),covB=vec3(t2.zw,t3.x);
    tint=color;
    vec3 center=(view*vec4(position,1.0)).xyz;
    float z=-center.z;
    if(z<=nearPlane || z>=farPlane || color.a<0.0039) {
        gl_Position=vec4(2.0,2.0,2.0,1.0); gaussian=vec2(0.0); return;
    }
    float focal=viewport.y*1.20710678; // 45 degree vertical field of view
    mat3 covariance=mat3(covA.x,covA.y,covA.z,covA.y,covB.x,covB.y,covA.z,covB.y,covB.z);
    mat3 rotation=mat3(view);
    mat3 c=rotation*covariance*transpose(rotation);
    // Perspective projection derivative, screen coordinates in pixels.
    vec3 jx=vec3(focal/z,0.0,focal*clamp(center.x/z,-0.54*viewport.x/viewport.y,0.54*viewport.x/viewport.y)/z);
    vec3 jy=vec3(0.0,focal/z,focal*clamp(center.y/z,-0.54,0.54)/z);
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
    gaussian=corners[gl_VertexID]*3.0;
    vec2 offset=axis*min(sqrt(l1),4096.0)*gaussian.x+vec2(-axis.y,axis.x)*min(sqrt(l2),4096.0)*gaussian.y;
    vec2 ndc=(focal*center.xy/z+offset)*2.0/viewport;
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
    sorter_=std::thread(&Renderer::SortLoop,this);
    worker_=std::thread(&Renderer::Loop,this,window);
}
void Renderer::Stop() {
    { std::lock_guard<std::mutex> lock(mutex_); stop_=true; cancel_=true; }
    changed_.notify_all();sortChanged_.notify_all(); if(worker_.joinable()) worker_.join();if(sorter_.joinable())sorter_.join();
    sortPending_=sortReady_=false;sortScene_.reset();sortedScene_.reset();sortedIndices_.clear();
}
void Renderer::SortLoop() {
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
    {std::lock_guard<std::mutex> lock(mutex_);chunkPaths_.clear();chunksDirty_=false;requestedPath_=path;pendingPath_=std::move(path);cancel_=true;status_.state="loading";status_.message="Loading model";}
    changed_.notify_one();
}
void Renderer::SetChunks(std::vector<std::string> paths, std::array<float,4> bounds, std::vector<uint32_t> ranges) {
    {std::lock_guard<std::mutex> lock(mutex_);chunkPaths_=std::move(paths);chunkBounds_=bounds;chunkRanges_=std::move(ranges);
     pendingPath_.clear();requestedPath_.clear();chunksDirty_=!chunkPaths_.empty();cancel_=true;dirty_=true;
     status_.state=chunkPaths_.empty()?"ready":"loading";status_.message=chunkPaths_.empty()?"Stream stopped":"Streaming chunks";}
    changed_.notify_one();
}
void Renderer::SetCamera(Camera camera) {
    {std::lock_guard<std::mutex> lock(mutex_);camera_=camera;dirty_=true;}changed_.notify_one();
}
void Renderer::SetActive(bool active) { {std::lock_guard<std::mutex> lock(mutex_);active_=active;dirty_=true;}changed_.notify_one(); }
std::vector<float> Renderer::Pick(float x,float y) {
    std::shared_ptr<Scene> scene;Camera camera;int width,height;
    {std::lock_guard<std::mutex> lock(mutex_);scene=scene_;camera=camera_;width=width_;height=height_;}
    return splat::Pick(*scene,MakeView(*scene,camera),x,y,width,height);
}
Status Renderer::GetStatus() {std::lock_guard<std::mutex> lock(mutex_);status_.width=width_;status_.height=height_;return status_;}
void Renderer::InitGL(void *window) {
    uploadDirty_=true; window_ = window; bufferWidth_ = bufferHeight_ = 0;
    display_=eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if(display_==EGL_NO_DISPLAY || !eglInitialize(display_,nullptr,nullptr)) throw std::runtime_error("EGL display initialization failed");
    const EGLint configAttrs[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
    EGLConfig config; EGLint count;
    if(!eglChooseConfig(display_,configAttrs,&config,1,&count)||!count) throw std::runtime_error("OpenGL ES 3 window configuration unavailable");
    const EGLint attrs[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    context_=eglCreateContext(display_,config,EGL_NO_CONTEXT,attrs);
    surface_=eglCreateWindowSurface(display_,config,reinterpret_cast<EGLNativeWindowType>(window),nullptr);
    if(context_==EGL_NO_CONTEXT||surface_==EGL_NO_SURFACE||!eglMakeCurrent(display_,surface_,surface_,context_)) throw std::runtime_error("EGL surface/context creation failed");
    eglSwapInterval(display_,1);
    const auto *version=glGetString(GL_VERSION), *renderer=glGetString(GL_RENDERER);
    {std::lock_guard<std::mutex> lock(mutex_);status_.graphics=std::string(version?reinterpret_cast<const char*>(version):"unknown")+" / "+(renderer?reinterpret_cast<const char*>(renderer):"unknown");}
    GLuint vertex=Compile(GL_VERTEX_SHADER,Vertex), fragment=0;
    try {fragment=Compile(GL_FRAGMENT_SHADER,Fragment);} catch(...) {glDeleteShader(vertex);throw;}
    program_=glCreateProgram();glAttachShader(program_,vertex);glAttachShader(program_,fragment);glLinkProgram(program_);glDeleteShader(vertex);glDeleteShader(fragment);
    GLint ok;glGetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok) throw std::runtime_error("Gaussian shader link failed");
    glGenVertexArrays(1,&vao_);glBindVertexArray(vao_);glGenBuffers(1,&buffer_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
    glEnableVertexAttribArray(0);glVertexAttribIPointer(0,1,GL_UNSIGNED_INT,sizeof(uint32_t),nullptr);glVertexAttribDivisor(0,1);
    GLint maxTexture=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxTexture);if(maxTexture<4096)throw std::runtime_error("4096-wide data textures unavailable");
    glGenTextures(1,&dataTexture_);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,dataTexture_);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
}
void Renderer::DestroyGL() {
    if(display_==EGL_NO_DISPLAY)return;
    if(context_!=EGL_NO_CONTEXT && surface_!=EGL_NO_SURFACE){
        eglMakeCurrent(display_,surface_,surface_,context_);
        if(dataTexture_)glDeleteTextures(1,&dataTexture_);
        if(buffer_)glDeleteBuffers(1,&buffer_);if(vao_)glDeleteVertexArrays(1,&vao_);if(program_)glDeleteProgram(program_);
    }
    dataTexture_=buffer_=vao_=program_=0;eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    if(surface_!=EGL_NO_SURFACE)eglDestroySurface(display_,surface_);
    if(context_!=EGL_NO_CONTEXT)eglDestroyContext(display_,context_);
    eglTerminate(display_);display_=EGL_NO_DISPLAY;surface_=EGL_NO_SURFACE;context_=EGL_NO_CONTEXT;
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
    if(uploadDirty_) {sorted=SortIndices(*scene_,view);sortMs=Ms(start);sortDirection_=direction;}
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
    glViewport(0,0,width,height);glClearColor(.035f,.045f,.065f,1);glClear(GL_COLOR_BUFFER_BIT);glUseProgram(program_);
    glUniformMatrix4fv(glGetUniformLocation(program_,"view"),1,GL_FALSE,view.matrix.data());
    glUniform2f(glGetUniformLocation(program_,"viewport"),float(width),float(height));
    glUniform1f(glGetUniformLocation(program_,"nearPlane"),view.nearPlane);glUniform1f(glGetUniformLocation(program_,"farPlane"),view.farPlane);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,dataTexture_);glUniform1i(glGetUniformLocation(program_,"splatData"),0);
    if(uploadDirty_) {
        const size_t height=std::max(size_t(1),(scene_->points.size()*4+4095)/4096);
        std::vector<float> pixels(height*4096*4,0);
        for(size_t i=0;i<scene_->points.size();++i) {const auto &g=scene_->points[i];float *p=pixels.data()+i*16;
            std::copy_n(g.position,3,p);std::copy_n(g.color,4,p+3);std::copy_n(g.covariance,6,p+7);}
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,4096,height,0,GL_RGBA,GL_FLOAT,pixels.data());
    }
    glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);if(resort){glBufferData(GL_ARRAY_BUFFER,sorted.size()*sizeof(uint32_t),sorted.data(),GL_DYNAMIC_DRAW);uploadDirty_=false;}
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,GLsizei(scene_->points.size()));
    const GLenum error=glGetError();if(error!=GL_NO_ERROR)throw std::runtime_error("OpenGL error: "+std::to_string(error));
    if(!eglSwapBuffers(display_,surface_))throw std::runtime_error("EGL swap failed");
    std::lock_guard<std::mutex> lock(mutex_);status_.frames++;status_.sortMs=sortMs;status_.frameMs=Ms(drawStart);status_.fps=1000.0/std::max(Ms(start),.001);size_t cached=0;for(const auto &item:decoded_)cached+=item.second->points.capacity();for(const auto &item:selected_)cached+=item.second.second->points.capacity();status_.bytes=(cached+scene_->points.capacity())*sizeof(Gaussian)+scene_->points.size()*68+sorted.capacity()*sizeof(uint32_t);
}
void Renderer::Loop(void *window) {
    try {
        InitGL(window);
        for(;;) {
            std::string path;Camera camera;int width,height;std::vector<std::string> chunks;std::array<float,4> bounds{};std::vector<uint32_t> ranges;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                changed_.wait(lock,[&]{return stop_||(active_&&(dirty_||!pendingPath_.empty()||chunksDirty_));});
                if(stop_)break;
                path=std::move(pendingPath_);pendingPath_.clear();camera=camera_;width=width_;height=height_;dirty_=false;
                if(chunksDirty_){chunks=chunkPaths_;bounds=chunkBounds_;ranges=chunkRanges_;chunksDirty_=false;}
                if(!path.empty()||!chunks.empty())cancel_=false;
            }
            if(!path.empty()) {
                const auto start=Clock::now();
                try {
                    decoded_.clear();selected_.clear();auto loaded=ReadModel(path,&cancel_);
                    std::lock_guard<std::mutex> lock(mutex_);
                    if(cancel_.load())continue;
                    scene_=std::make_shared<Scene>(std::move(loaded));uploadDirty_=true;sortPending_=sortReady_=false;sortScene_.reset();sortedScene_.reset();sortedIndices_.clear();camera_=Camera{};camera=camera_;
                    status_.count=scene_->points.size();status_.loadMs=Ms(start);status_.state="ready";status_.message="Model loaded";
                } catch(const std::exception &e) {
                    std::lock_guard<std::mutex> lock(mutex_);if(!cancel_){status_.state="error";status_.message=e.what();}
                }
            }
            if(!chunks.empty()) {
                const auto start=Clock::now();
                try {
                    Scene combined;size_t requested=0;for(size_t i=2;i<ranges.size();i+=3)requested+=ranges[i];if(requested>MaxGaussians)throw std::runtime_error("Resident budget exceeded");combined.points.reserve(requested);combined.center={bounds[0],bounds[1],bounds[2]};combined.radius=bounds[3];
                    if(ranges.empty())for(size_t i=0;i<chunks.size();++i){ranges.push_back(i);ranges.push_back(0);ranges.push_back(0);}
                    for(size_t file=0;file<chunks.size();++file) {
                        if(cancel_)throw std::runtime_error("Load cancelled");
                        std::vector<uint32_t> selection;
                        for(size_t i=0;i<ranges.size();i+=3)if(ranges[i]==file){selection.push_back(ranges[i+1]);selection.push_back(ranges[i+2]);}
                        auto cachedSelection=selected_.find(chunks[file]);std::shared_ptr<Scene> subset;
                        if(cachedSelection!=selected_.end()&&cachedSelection->second.first==selection)subset=cachedSelection->second.second;
                        else {
                            auto found=decoded_.find(chunks[file]);std::shared_ptr<Scene> part;
                            if(found!=decoded_.end())part=found->second;
                            else {part=std::make_shared<Scene>(ReadModel(chunks[file],&cancel_));decoded_[chunks[file]]=part;}
                            subset=std::make_shared<Scene>();
                            for(size_t i=0;i<selection.size();i+=2){
                                const size_t offset=selection[i],count=selection[i+1]?selection[i+1]:part->points.size();
                                if(offset>part->points.size()||count>part->points.size()-offset||subset->points.size()+count>MaxGaussians)throw std::runtime_error("LOD selection exceeds model or resident budget");
                                subset->points.insert(subset->points.end(),part->points.begin()+offset,part->points.begin()+offset+count);
                            }
                            selected_[chunks[file]]={selection,subset};
                        }
                        if(combined.points.size()+subset->points.size()>MaxGaussians)throw std::runtime_error("Resident budget exceeded");
                        combined.points.insert(combined.points.end(),subset->points.begin(),subset->points.end());
                        size_t cached=0;for(const auto &item:decoded_)cached+=item.second->points.size();
                        while(cached>4000000&&decoded_.size()>1) {
                            auto evict=decoded_.begin();if(evict->first==chunks[file])++evict;
                            cached-=evict->second->points.size();decoded_.erase(evict);
                        }
                    }
                    for(auto i=selected_.begin();i!=selected_.end();) {
                        if(std::find(chunks.begin(),chunks.end(),i->first)==chunks.end())i=selected_.erase(i);else ++i;
                    }
                    std::lock_guard<std::mutex> lock(mutex_);
                    if(cancel_)continue;
                    scene_=std::make_shared<Scene>(std::move(combined));uploadDirty_=true;sortPending_=sortReady_=false;sortScene_.reset();sortedScene_.reset();sortedIndices_.clear();camera=camera_;status_.count=scene_->points.size();
                    status_.loadMs=Ms(start);status_.state="ready";status_.message="Stream resident set ready";
                } catch(const std::exception &e) {
                    std::lock_guard<std::mutex> lock(mutex_);if(!cancel_){status_.state="error";status_.message=e.what();}
                }
            }
            Draw(MakeView(*scene_,camera),width,height);
            {std::lock_guard<std::mutex> lock(mutex_);if(status_.state=="waiting"){status_.state="ready";status_.message="OpenGL ES surface ready";}}
        }
    }catch(const std::exception &e){std::lock_guard<std::mutex> lock(mutex_);status_.state="error";status_.message=e.what();}
    DestroyGL();
}
}
