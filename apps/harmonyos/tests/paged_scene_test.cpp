#include "splat.h"
#include <cassert>
#include <iostream>
#include <random>
#include <numeric>
using namespace splat;
int main(){
    auto source=std::make_shared<Scene>();
    for(int i=0;i<6;++i){Gaussian g{};g.position[2]=float(i-3);g.color[3]=.8f;g.covariance[0]=g.covariance[3]=g.covariance[5]=.01f;source->points.push_back(g);}
    Scene flat,paged;paged.paged=true;
    for(auto pair:{std::pair<int,int>{3,2},{0,3}}){
        paged.ranges.push_back({source,uint32_t(pair.first),uint32_t(pair.second),uint32_t(paged.Count())});
        for(int i=pair.first;i<pair.first+pair.second;++i){const auto &g=source->points[i];flat.points.push_back(g);paged.positions.push_back({g.position[0],g.position[1],g.position[2]});paged.addresses.push_back(i+100);}
    }
    for(size_t i=0;i<flat.Count();++i)assert(flat.At(i).position[2]==paged.At(i).position[2]);
    const auto view=MakeView(flat,Camera{});
    assert(SortIndices(flat,view)==SortIndices(paged,view));
    assert(Pick(flat,view,.5,.5,512,512)==Pick(paged,view,.5,.5,512,512));
    Scene random;random.points.resize(100001);std::mt19937 rng(42);std::uniform_real_distribution<float> dist(-1000,1000);
    for(auto &g:random.points){g.position[2]=dist(rng);}
    random.points[1].position[2]=random.points[0].position[2];
    random.points[2].position[2]=-0.0f;random.points[3].position[2]=0.0f;
    View axis{};axis.matrix[10]=1;
    std::vector<uint32_t> expected(random.Count());std::iota(expected.begin(),expected.end(),0);
    std::stable_sort(expected.begin(),expected.end(),[&](uint32_t a,uint32_t b){return random.Position(a)[2]<random.Position(b)[2];});
    assert(SortIndices(random,axis)==expected);
    assert(SortIndices(flat,view)==SortIndices(paged,view)); // shrink/reuse thread workspace
    std::cout<<"PASS paged ordering and covariance-aware picking match the flat scene\n";
}
