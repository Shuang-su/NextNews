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
layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
layout(location=2) in vec3 covA;
layout(location=3) in vec3 covB;
uniform mat4 view;
uniform vec2 viewport;
uniform float nearPlane;
uniform float farPlane;
out vec2 gaussian;
out vec4 tint;
void main() {
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
      if(status_.state=="loading"&&pendingPath_.empty())pendingPath_=requestedPath_;
    }
    worker_=std::thread(&Renderer::Loop,this,window);
}
void Renderer::Stop() {
    { std::lock_guard<std::mutex> lock(mutex_); stop_=true; cancel_=true; }
    changed_.notify_all(); if(worker_.joinable()) worker_.join();
}
void Renderer::Resize(int width,int height) { {std::lock_guard<std::mutex> lock(mutex_);width_=std::max(1,width);height_=std::max(1,height);dirty_=true;}changed_.notify_one(); }
void Renderer::Load(std::string path) {
    {std::lock_guard<std::mutex> lock(mutex_);requestedPath_=path;pendingPath_=std::move(path);cancel_=true;status_.state="loading";status_.message="Loading model";}
    changed_.notify_one();
}
void Renderer::SetCamera(Camera camera) {
    {std::lock_guard<std::mutex> lock(mutex_);camera_=camera;dirty_=true;}changed_.notify_one();
}
void Renderer::SetActive(bool active) { {std::lock_guard<std::mutex> lock(mutex_);active_=active;dirty_=true;}changed_.notify_one(); }
Status Renderer::GetStatus() {std::lock_guard<std::mutex> lock(mutex_);return status_;}
void Renderer::InitGL(void *window) {
    window_ = window; bufferWidth_ = bufferHeight_ = 0;
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
    const size_t offsets[]={offsetof(Gaussian,position),offsetof(Gaussian,color),offsetof(Gaussian,covariance),offsetof(Gaussian,covariance)+3*sizeof(float)};
    const GLint sizes[]={3,4,3,3};
    for(GLuint i=0;i<4;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,sizes[i],GL_FLOAT,GL_FALSE,sizeof(Gaussian),reinterpret_cast<const void*>(offsets[i]));glVertexAttribDivisor(i,1);}
    glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);
}
void Renderer::DestroyGL() {
    if(display_==EGL_NO_DISPLAY)return;
    if(context_!=EGL_NO_CONTEXT && surface_!=EGL_NO_SURFACE){
        eglMakeCurrent(display_,surface_,surface_,context_);
        if(buffer_)glDeleteBuffers(1,&buffer_);if(vao_)glDeleteVertexArrays(1,&vao_);if(program_)glDeleteProgram(program_);
    }
    buffer_=vao_=program_=0;eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    if(surface_!=EGL_NO_SURFACE)eglDestroySurface(display_,surface_);
    if(context_!=EGL_NO_CONTEXT)eglDestroyContext(display_,context_);
    eglTerminate(display_);display_=EGL_NO_DISPLAY;surface_=EGL_NO_SURFACE;context_=EGL_NO_CONTEXT;
}
void Renderer::Draw(const View &view,int width,int height) {
    if(bufferWidth_!=width || bufferHeight_!=height) {
        const int result=OH_NativeWindow_NativeWindowHandleOpt(static_cast<OHNativeWindow*>(window_),SET_BUFFER_GEOMETRY,width,height);
        if(result!=0)throw std::runtime_error("Native window buffer resize failed: "+std::to_string(result));
        bufferWidth_=width;bufferHeight_=height;
    }
    const auto start=Clock::now();auto sorted=Sort(scene_,view);const double sortMs=Ms(start);
    const auto drawStart=Clock::now();
    glViewport(0,0,width,height);glClearColor(.035f,.045f,.065f,1);glClear(GL_COLOR_BUFFER_BIT);glUseProgram(program_);
    glUniformMatrix4fv(glGetUniformLocation(program_,"view"),1,GL_FALSE,view.matrix.data());
    glUniform2f(glGetUniformLocation(program_,"viewport"),float(width),float(height));
    glUniform1f(glGetUniformLocation(program_,"nearPlane"),view.nearPlane);glUniform1f(glGetUniformLocation(program_,"farPlane"),view.farPlane);
    glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);glBufferData(GL_ARRAY_BUFFER,sorted.size()*sizeof(Gaussian),sorted.data(),GL_DYNAMIC_DRAW);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,GLsizei(sorted.size()));
    const GLenum error=glGetError();if(error!=GL_NO_ERROR)throw std::runtime_error("OpenGL error: "+std::to_string(error));
    if(!eglSwapBuffers(display_,surface_))throw std::runtime_error("EGL swap failed");
    std::lock_guard<std::mutex> lock(mutex_);status_.sortMs=sortMs;status_.frameMs=Ms(drawStart);status_.fps=1000.0/std::max(Ms(start),.001);status_.bytes=(scene_.points.capacity()+sorted.capacity())*sizeof(Gaussian)+scene_.points.size()*sizeof(Gaussian);
}
void Renderer::Loop(void *window) {
    try {
        InitGL(window);
        for(;;) {
            std::string path;Camera camera;int width,height;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                changed_.wait(lock,[&]{return stop_||(active_&&(dirty_||!pendingPath_.empty()));});
                if(stop_)break;
                path=std::move(pendingPath_);pendingPath_.clear();camera=camera_;width=width_;height=height_;dirty_=false;
                if(!path.empty())cancel_=false;
            }
            if(!path.empty()) {
                const auto start=Clock::now();
                try {
                    auto loaded=ReadPly(path,&cancel_);
                    std::lock_guard<std::mutex> lock(mutex_);
                    if(cancel_.load())continue;
                    scene_=std::move(loaded);camera_=Camera{};camera=camera_;
                    status_.count=scene_.points.size();status_.loadMs=Ms(start);status_.state="ready";status_.message="Model loaded";
                } catch(const std::exception &e) {
                    std::lock_guard<std::mutex> lock(mutex_);if(!cancel_){status_.state="error";status_.message=e.what();}
                }
            }
            Draw(MakeView(scene_,camera),width,height);
            {std::lock_guard<std::mutex> lock(mutex_);if(status_.state=="waiting"){status_.state="ready";status_.message="OpenGL ES surface ready";}}
        }
    }catch(const std::exception &e){std::lock_guard<std::mutex> lock(mutex_);status_.state="error";status_.message=e.what();}
    DestroyGL();
}
}
