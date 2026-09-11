#include "splat.h"
#include <nlohmann/json.hpp>
#include <webp/decode.h>
#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
namespace splat {
namespace {
using Bytes=std::vector<uint8_t>;
uint16_t U16(const Bytes &b,size_t p){if(p+2>b.size())throw std::runtime_error("Truncated SOG ZIP");return b[p]|uint16_t(b[p+1])<<8;}
uint32_t U32(const Bytes &b,size_t p){return U16(b,p)|uint32_t(U16(b,p+2))<<16;}
std::map<std::string,Bytes> Unzip(const std::string &path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot open SOG");
    const auto n=f.tellg();if(n<22||size_t(n)>MaxFileBytes)throw std::runtime_error("SOG size limit");
    Bytes b(static_cast<size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("SOG read failed");
    size_t end=b.size()-22;const size_t lower=b.size()>65557?b.size()-65557:0;
    while(U32(b,end)!=0x06054b50){if(end==lower)throw std::runtime_error("SOG ZIP directory missing");--end;}
    if(U16(b,end+4)||U16(b,end+6)||U16(b,end+8)!=U16(b,end+10)||end+22+U16(b,end+20)!=b.size())throw std::runtime_error("Unsupported SOG ZIP");
    const size_t entries=U16(b,end+10);size_t p=U32(b,end+16),total=0;
    if(entries>32||p+U32(b,end+12)!=end)throw std::runtime_error("SOG ZIP directory bounds");
    std::map<std::string,Bytes> files;
    for(size_t i=0;i<entries;++i){
        if(U32(b,p)!=0x02014b50)throw std::runtime_error("Invalid SOG ZIP entry");
        const size_t packed=U32(b,p+20),size=U32(b,p+24),len=U16(b,p+28),local=U32(b,p+42);
        const unsigned method=U16(b,p+10);const auto crc=U32(b,p+16);
        const size_t next=p+46+len+U16(b,p+30)+U16(b,p+32);
        if(next>end||U16(b,p+8)&1||size>32*1024*1024||(total+=size)>128*1024*1024)throw std::runtime_error("SOG ZIP resource limit");
        std::string name(reinterpret_cast<char*>(b.data()+p+46),len);
        if(name.empty()||name.find_first_of("/\\")!=std::string::npos||name.find('\0')!=std::string::npos||files.count(name))throw std::runtime_error("Invalid SOG filename");
        if(U32(b,local)!=0x04034b50)throw std::runtime_error("SOG ZIP local header");
        const size_t start=local+30+U16(b,local+26)+U16(b,local+28);
        if(start>p||packed>p-start)throw std::runtime_error("SOG ZIP payload bounds");
        Bytes out(size);
        if(method==0){if(packed!=size)throw std::runtime_error("SOG ZIP stored length");std::copy_n(b.data()+start,size,out.data());}
        else if(method==8){z_stream z{};z.next_in=b.data()+start;z.avail_in=packed;z.next_out=out.data();z.avail_out=size;
            if(inflateInit2(&z,-MAX_WBITS)!=Z_OK)throw std::runtime_error("SOG inflate init");
            const int result=inflate(&z,Z_FINISH);inflateEnd(&z);
            if(result!=Z_STREAM_END||z.total_out!=size||z.total_in!=packed)throw std::runtime_error("SOG ZIP deflate data");}
        else throw std::runtime_error("Unsupported SOG ZIP compression");
        if(crc32(0,out.data(),out.size())!=crc)throw std::runtime_error("SOG ZIP checksum mismatch");
        files.emplace(name,std::move(out));p=next;
    }
    return files;
}
Bytes Pixels(const std::map<std::string,Bytes> &files,const std::string &name,size_t count,int &width,int &height) {
    const auto &b=files.at(name);int w=0,h=0;
    if(!WebPGetInfo(b.data(),b.size(),&w,&h)||w<1||h<1||size_t(w)*h<count||size_t(w)*h>MaxGaussians+16384)throw std::runtime_error("SOG texture dimensions");
    if(width&&(w!=width||h!=height))throw std::runtime_error("SOG texture dimensions mismatch");width=w;height=h;
    Bytes out(size_t(w)*h*4);
    if(!WebPDecodeRGBAInto(b.data(),b.size(),out.data(),out.size(),w*4))throw std::runtime_error("SOG WebP decode failed");
    return out;
}
}
Scene ReadSog(const std::string &path,const std::atomic<bool> *cancel) {
    const auto files=Unzip(path);const auto &mb=files.at("meta.json");
    if(mb.size()>1024*1024)throw std::runtime_error("SOG metadata too large");
    auto m=nlohmann::json::parse(mb.begin(),mb.end(),[](int depth,nlohmann::json::parse_event_t,nlohmann::json &){if(depth>32)throw std::runtime_error("SOG metadata nesting limit");return true;});
    if(m.at("version")!=2)throw std::runtime_error("SOG v2 required");
    const int count=m.at("count").get<int>();if(count<=0||size_t(count)>MaxGaussians)throw std::runtime_error("SOG Gaussian limit");
    const auto lo=m.at("means").at("mins").get<std::vector<float>>(),hi=m.at("means").at("maxs").get<std::vector<float>>();
    const auto sc=m.at("scales").at("codebook").get<std::vector<float>>(),sh=m.at("sh0").at("codebook").get<std::vector<float>>();
    if(lo.size()!=3||hi.size()!=3||sc.size()!=256||sh.size()!=256)throw std::runtime_error("SOG metadata shape");
    for(int k=0;k<3;++k)if(!std::isfinite(lo[k])||!std::isfinite(hi[k])||lo[k]>hi[k]||std::abs(lo[k])>13.8||std::abs(hi[k])>13.8)throw std::runtime_error("SOG means range");
    for(auto v:sc)if(!std::isfinite(v)||v< -30||v>14)throw std::runtime_error("SOG scale range");
    for(auto v:sh)if(!std::isfinite(v)||std::abs(v)>1e6)throw std::runtime_error("SOG SH0 range");
    int width=0,height=0;
    auto names=m.at("means").at("files").get<std::vector<std::string>>();if(names.size()!=2)throw std::runtime_error("SOG means files");
    auto low=Pixels(files,names[0],count,width,height),high=Pixels(files,names[1],count,width,height);
    auto quat=Pixels(files,m.at("quats").at("files").at(0).get<std::string>(),count,width,height);
    auto scale=Pixels(files,m.at("scales").at("files").at(0).get<std::string>(),count,width,height);
    auto color=Pixels(files,m.at("sh0").at("files").at(0).get<std::string>(),count,width,height);
    Scene scene;scene.points.resize(count);float mins[3]={INFINITY,INFINITY,INFINITY},maxs[3]={-INFINITY,-INFINITY,-INFINITY};
    for(int i=0;i<count;++i){if(cancel&&cancel->load())throw std::runtime_error("Load cancelled");auto &g=scene.points[i];
        for(int k=0;k<3;++k){const double t=(uint16_t(high[i*4+k])<<8|low[i*4+k])/65535.;const double v=lo[k]*(1-t)+hi[k]*t;
            g.position[k]=std::copysign(std::expm1(std::abs(v)),v);mins[k]=std::min(mins[k],g.position[k]);maxs[k]=std::max(maxs[k],g.position[k]);
            g.color[k]=std::clamp(.5f+.28209479177387814f*sh[color[i*4+k]],0.f,1.f);}
        g.color[3]=color[i*4+3]/255.f;
        const float a=(quat[i*4]/255.f-.5f)*1.41421356237f,b=(quat[i*4+1]/255.f-.5f)*1.41421356237f,c=(quat[i*4+2]/255.f-.5f)*1.41421356237f;
        const float d=std::sqrt(std::max(0.f,1-a*a-b*b-c*c));float w,x,y,z;
        switch(quat[i*4+3]){case 252:w=d;x=a;y=b;z=c;break;case 253:w=a;x=d;y=b;z=c;break;case 254:w=a;x=b;y=d;z=c;break;case 255:w=a;x=b;y=c;z=d;break;default:throw std::runtime_error("SOG quaternion mode");}
        const float inv=1/std::sqrt(w*w+x*x+y*y+z*z);w*=inv;x*=inv;y*=inv;z*=inv;
        const float r[3][3]={{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)}, {2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)}, {2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}};
        float cov[3][3]{};for(int u=0;u<3;++u)for(int v=0;v<3;++v)for(int k=0;k<3;++k)cov[u][v]+=r[u][k]*r[v][k]*std::exp(2*sc[scale[i*4+k]]);
        const float packed[]={cov[0][0],cov[0][1],cov[0][2],cov[1][1],cov[1][2],cov[2][2]};std::copy_n(packed,6,g.covariance);
    }
    double radius2=0;for(int k=0;k<3;++k){scene.center[k]=(mins[k]+maxs[k])*.5f;radius2+=double(maxs[k]-mins[k])*(maxs[k]-mins[k])*.25;}
    scene.radius=std::max(.001f,float(std::sqrt(radius2)));return scene;
}
Scene ReadModel(const std::string &path,const std::atomic<bool> *cancel) {
    auto scene=path.size()>=4&&path.substr(path.size()-4)==".sog"?ReadSog(path,cancel):ReadPly(path,cancel);
    ApplyViewerTransform(scene);return scene;
}
}
