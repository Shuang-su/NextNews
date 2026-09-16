#include "hotspots.h"
#include "post_shaders.h"
#include <stdexcept>
#include <hilog/log.h>
namespace splat {
namespace {
const char *Vertex=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 position;
uniform mat4 view;
uniform vec2 viewport;
uniform float nearPlane,farPlane,tanHalfFov,pixels;
out vec2 local;
flat out int label;
void main(){
    vec2 corners[4]=vec2[4](vec2(-1,-1),vec2(1,-1),vec2(-1,1),vec2(1,1));
    local=corners[gl_VertexID];label=gl_InstanceID+1;
    vec3 c=(view*vec4(position,1)).xyz;float z=-c.z;
    if(z<=0.000001){gl_Position=vec4(0,0,2,1);return;}
    vec2 center=c.xy*viewport.y/(2.0*tanHalfFov*z);
    float depth=(farPlane+nearPlane)/(farPlane-nearPlane)-2.0*farPlane*nearPlane/((farPlane-nearPlane)*z);
    gl_Position=vec4((center+local*pixels*.5)*2.0/viewport,clamp(depth,-1.0,1.0),1);
})GLSL";
const char *Fragment=R"GLSL(#version 300 es
precision highp float;
uniform highp sampler2D glyphs;
uniform float opacity;
uniform int hover;
uniform int directTone;
/*TONE_FUNCTIONS*/
in vec2 local;
flat in int label;
out vec4 color;
void main(){
    vec2 p=local*32.0;float r=length(p),aa=max(fwidth(r),.35);
    float a=1.0-smoothstep(31.0-aa,31.0+aa,r);if(a<.01)discard;
    float ink=smoothstep(25.0-aa,25.0+aa,r);
    int digits=label<10?1:label<100?2:label<1000?3:4;
    float scale=min(1.0,52.0/(float(digits)*19.0));vec2 t=p/scale;
    float width=float(digits)*19.0,x=t.x+width*.5;
    if(x>=0.0&&x<width&&abs(t.y)<16.0){
        int slot=int(floor(x/19.0)),divisor=int(pow(10.0,float(digits-1-slot))+.5);
        int digit=(label/divisor)%10;
        vec2 uv=vec2((float(digit)*32.0+mod(x,19.0)+6.5)/320.0,(16.0-t.y)/32.0);
        ink=max(ink,texture(glyphs,uv).r);
    }
    vec3 tint=label-1==hover?vec3(1,.4,0):vec3(.8);
    tint*=ink;
    // MetaFlow StandardMaterial maps emissive color before alpha blending when
    // CameraFrame is absent. CameraFrame applies tone mapping during composition.
    if(directTone>1){
        vec3 linear=pow(max(tint,vec3(0)),vec3(2.2));
        if(directTone==2)linear=toneMap2(linear);else if(directTone==3)linear=toneMap3(linear);
        else if(directTone==4)linear=toneMap4(linear);else if(directTone==5)linear=toneMap5(linear);else if(directTone==6)linear=toneMap6(linear);
        tint=pow(max(linear,vec3(0))+0.0000001,vec3(1.0/2.2));
    }
    a*=opacity;color=vec4(tint*a,a);
})GLSL";
GLuint Shader(GLenum kind,const char *source){GLuint s=glCreateShader(kind);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){glDeleteShader(s);throw std::runtime_error("Hotspot shader compile failed");}return s;}
}
bool Hotspots::Prepare(const std::shared_ptr<const HotspotData>& data){
    if(!data||data->positions.empty()){uploaded_.reset();return false;}
    if(failed_)return false;
    try{
        if(!program_){
            std::string fragment=Fragment;const std::string marker="/*TONE_FUNCTIONS*/";fragment.replace(fragment.find(marker),marker.size(),PostTone);
            GLuint v=Shader(GL_VERTEX_SHADER,Vertex),f=0;
            try{f=Shader(GL_FRAGMENT_SHADER,fragment.c_str());}catch(...){glDeleteShader(v);throw;}
            program_=glCreateProgram();glAttachShader(program_,v);glAttachShader(program_,f);glLinkProgram(program_);glDeleteShader(v);glDeleteShader(f);
            GLint ok=0;glGetProgramiv(program_,GL_LINK_STATUS,&ok);if(!ok)throw std::runtime_error("Hotspot program link failed");
            glGenTextures(1,&texture_);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,texture_);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glGenVertexArrays(1,&vao_);glBindVertexArray(vao_);glGenBuffers(1,&buffer_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
            glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);glVertexAttribDivisor(0,1);
        }
        if(uploaded_!=data){
            glBindBuffer(GL_ARRAY_BUFFER,buffer_);glBufferData(GL_ARRAY_BUFFER,data->positions.size()*sizeof(float),data->positions.data(),GL_STATIC_DRAW);
            glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,texture_);glTexImage2D(GL_TEXTURE_2D,0,GL_R8,320,32,0,GL_RED,GL_UNSIGNED_BYTE,data->glyphs.data());uploaded_=data;
        }
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Hotspot GPU resources failed");
        return true;
    }catch(const std::exception &e){OH_LOG_Print(LOG_APP,LOG_ERROR,0xD003,"NextNewsViewer","%{public}s",e.what());Destroy();failed_=true;return false;}
}
void Hotspots::Draw(const View&v,int width,int height,HotspotStyle s,bool overlay,int directTone){
    if(!program_||!uploaded_||!s.visible)return;
    glUseProgram(program_);glBindVertexArray(vao_);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,texture_);
    glUniform1i(glGetUniformLocation(program_,"directTone"),directTone);
    glUniform1i(glGetUniformLocation(program_,"glyphs"),3);glUniform1i(glGetUniformLocation(program_,"hover"),s.hover);
    glUniformMatrix4fv(glGetUniformLocation(program_,"view"),1,GL_FALSE,v.matrix.data());
    glUniform2f(glGetUniformLocation(program_,"viewport"),width,height);
    glUniform1f(glGetUniformLocation(program_,"nearPlane"),v.nearPlane);glUniform1f(glGetUniformLocation(program_,"farPlane"),v.farPlane);
    glUniform1f(glGetUniformLocation(program_,"tanHalfFov"),v.tanHalfFov);glUniform1f(glGetUniformLocation(program_,"pixels"),s.pixels);
    glUniform1f(glGetUniformLocation(program_,"opacity"),overlay?.25f:1.f);
    if(overlay){glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);}else{glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);}
    glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP,0,4,uploaded_->positions.size()/3);
}
void Hotspots::Destroy(){if(program_)glDeleteProgram(program_);if(texture_)glDeleteTextures(1,&texture_);if(vao_)glDeleteVertexArrays(1,&vao_);if(buffer_)glDeleteBuffers(1,&buffer_);program_=texture_=vao_=buffer_=0;uploaded_.reset();failed_=false;}
}
