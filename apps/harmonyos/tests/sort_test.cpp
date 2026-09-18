#include "splat.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>

int main(int argc,char **argv) {
    const size_t count=argc>1?std::stoul(argv[1]):100003;
    std::mt19937 rng(731);
    std::uniform_real_distribution<float> random(-10000,10000);
    splat::Scene scene;scene.paged=true;scene.positions.resize(count);
    for(auto &p:scene.positions)for(auto &v:p)v=random(rng);
    // Equal keys, signed zero, very small and very large finite coordinates.
    for(size_t i=0;i<count;i+=17)scene.positions[i]={0,-0.f,0};
    if(count>3){scene.positions[1]={1e-30f,0,0};scene.positions[2]={-1e30f,0,0};}
    std::vector<uint32_t> expected(count);std::iota(expected.begin(),expected.end(),0);
    for(int pose=0;pose<5;++pose){
        splat::View view{};view.matrix[2]=pose?random(rng)/10000:1;
        view.matrix[6]=pose?random(rng)/10000:0;view.matrix[10]=pose?random(rng)/10000:0;
        auto depth=[&](uint32_t i){const auto*p=scene.Position(i);return view.matrix[2]*p[0]+view.matrix[6]*p[1]+view.matrix[10]*p[2];};
        std::iota(expected.begin(),expected.end(),0);
        std::stable_sort(expected.begin(),expected.end(),[&](auto a,auto b){return depth(a)<depth(b);});
        auto start=std::chrono::steady_clock::now();auto result=splat::SortIndices(scene,view);
        double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        assert(result==expected);std::cout<<count<<","<<pose<<","<<ms<<"\n";
    }
    splat::Scene empty;assert(splat::SortIndices(empty,{}).empty());
    // Also exercise the interleaved PLY representation and scratch shrinking.
    splat::Scene ply;ply.points.resize(257);
    for(size_t i=0;i<ply.Count();++i)std::copy_n(scene.Position(i%count),3,ply.points[i].position);
    splat::View view{};view.matrix[2]=1;
    auto indices=splat::SortIndices(ply,view);
    for(size_t i=1;i<indices.size();++i){auto a=indices[i-1],b=indices[i];assert(ply.Position(a)[0]<=ply.Position(b)[0]);if(ply.Position(a)[0]==ply.Position(b)[0])assert(a<b);}
}
