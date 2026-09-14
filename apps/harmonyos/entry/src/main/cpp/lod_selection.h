#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>
namespace splat {
struct LodTree {
    std::array<double,4> bounds{};
    std::vector<double> boxes;
    std::vector<int32_t> lods;
    uint32_t levels=0;
    std::vector<uint32_t> Select(const std::array<double,7>& camera,uint32_t budget,double fov,double aspect)const{
        const size_t n=boxes.size()/6;if(!n||!levels)return {};
        std::vector<uint32_t> selected(n),near(n);std::vector<double> distances(n);std::iota(near.begin(),near.end(),0);
        const double back[]={std::sin(camera[0])*std::cos(camera[1]),std::sin(camera[1]),std::cos(camera[0])*std::cos(camera[1])};
        double eye[3];for(int k=0;k<3;++k)eye[k]=bounds[k]+(camera[k+3]+(camera[6]!=0?0:back[k]*3*camera[2]))*bounds[3];
        const double ty=std::tan(std::clamp(fov,1.,178.)*3.14159265358979323846/360),scale=std::min(ty,ty*std::max(.1,aspect))/std::tan(3.14159265358979323846/6);
        auto item=[&](size_t i,uint32_t l,int field){return lods[(i*levels+l)*3+field];};
        int64_t count=0;
        for(size_t i=0;i<n;++i){
            double v[3];for(int k=0;k<3;++k)v[k]=std::clamp(eye[k],boxes[i*6+k],boxes[i*6+k+3])-eye[k];
            const double length=std::hypot(v[0],v[1],v[2]);
            const double behind=length>.01?std::max(0.,(v[0]*back[0]+v[1]*back[1]+v[2]*back[2])/length):0;
            distances[i]=std::max(.0001,length*(1+behind*4)*scale);
            selected[i]=std::min(levels-1,distances[i]<5?0u:uint32_t(std::floor(1+std::log(distances[i]/5)/std::log(3))));
            count+=item(i,selected[i],2);
        }
        std::stable_sort(near.begin(),near.end(),[&](uint32_t a,uint32_t b){return distances[a]<distances[b];});
        for(uint32_t pass=0;count>budget&&pass<levels;++pass){
            for(auto k=near.rbegin();k!=near.rend()&&count>budget;++k){const auto i=*k,l=selected[i];if(l+1>=levels)continue;count+=item(i,l+1,2)-item(i,l,2);++selected[i];}
        }
        for(uint32_t pass=0;pass<levels;++pass){
            bool changed=false;
            for(const auto i:near){const auto l=selected[i];if(!l)continue;const int64_t delta=int64_t(item(i,l-1,2))-item(i,l,2);
                if(item(i,l-1,0)>=0&&count+delta<=budget){count+=delta;--selected[i];changed=true;}}
            if(!changed)break;
        }
        return selected;
    }
};
}
