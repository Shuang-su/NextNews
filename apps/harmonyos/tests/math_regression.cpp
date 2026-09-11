#include "splat.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <random>
int main(){
 splat::Scene scene;scene.radius=1;scene.center={1,2,3};splat::Gaussian g{{2,3,4},{.2f,.3f,.4f,.8f},{1,.2f,.3f,2,.4f,3}};scene.points.push_back(g);
 splat::ApplyViewerTransform(scene);assert(scene.points[0].position[0]==-2&&scene.points[0].position[1]==-3&&scene.points[0].position[2]==4);assert(scene.points[0].covariance[1]==.2f&&scene.points[0].covariance[2]==-.3f&&scene.points[0].covariance[4]==-.4f);
 splat::ApplyViewerTransform(scene);assert(scene.points[0].position[0]==2&&scene.center[1]==2);
 scene.center={0,0,0};scene.points={{{0,0,0},{1,1,1,.1f},{.01f,0,0,.01f,0,.01f}},{{0,0,-1},{1,1,1,.9f},{.01f,0,0,.01f,0,.01f}}};
 auto p=splat::Pick(scene,splat::MakeView(scene,{}),.5f,.5f,100,100);assert(p.size()==3&&p[2]==-1);assert(splat::Pick(scene,splat::MakeView(scene,{}),0,0,100,100).empty());
 std::mt19937 rng(42);std::uniform_real_distribution<float> random(-100,100);scene.points.resize(260000);
 for(auto &v:scene.points)for(float &x:v.position)x=random(rng);
 auto view=splat::MakeView(scene,{.8f,.3f});const auto &m=view.matrix;
 auto start=std::chrono::steady_clock::now();auto actual=splat::SortIndices(scene,view);auto elapsed=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};const double radix=elapsed();
 std::vector<uint32_t> expected(scene.points.size());std::iota(expected.begin(),expected.end(),0);start=std::chrono::steady_clock::now();
 auto depth=[&](uint32_t i){const auto *p=scene.points[i].position;return m[2]*p[0]+m[6]*p[1]+m[10]*p[2];};
 std::stable_sort(expected.begin(),expected.end(),[&](auto a,auto b){return depth(a)<depth(b);});double reference=elapsed();assert(actual==expected);
 std::cout<<"PASS Rz180 covariance, translucent picking, empty-space picking, exact stable sort order; 260k radixMs="<<radix<<" referenceMs="<<reference<<"\n";
}
