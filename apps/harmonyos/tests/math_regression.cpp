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
 // Off-center clicks must unproject to that screen pixel, including rotated cameras.
 for(const auto yaw:{0.f,.5f}) {
   splat::Scene plane;plane.radius=1; splat::Gaussian g{};g.color[3]=.99f;g.covariance[0]=g.covariance[3]=g.covariance[5]=4;plane.points.push_back(g);
   splat::Camera camera;camera.yaw=yaw;auto v=splat::MakeView(plane,camera);
   for(float x:{.45f,.55f}){auto hit=splat::Pick(plane,v,x,.48f,400,800);assert(hit.size()==3);float q[3]={v.matrix[12],v.matrix[13],v.matrix[14]};
     for(int r=0;r<3;r++)for(int k=0;k<3;k++)q[r]+=v.matrix[k*4+r]*hit[k];
     const float focal=800/(2*v.tanHalfFov);assert(std::abs(.5f+focal*q[0]/(-q[2]*400)-x)<1e-5f);assert(std::abs(.5f-focal*q[1]/(-q[2]*800)-.48f)<1e-5f);
   }
 }

 splat::Camera wide;wide.fov=75;auto wideView=splat::MakeView(scene,wide);
 assert(std::abs(wideView.tanHalfFov-std::tan(75.f*.00872664626f))<1e-6f);
 assert(std::abs(splat::MakeView(scene,{}).tanHalfFov-std::tan(45.f*.00872664626f))<1e-6f);
 std::mt19937 rng(42);std::uniform_real_distribution<float> random(-100,100);scene.points.resize(260000);
 for(auto &v:scene.points)for(float &x:v.position)x=random(rng);
 auto view=splat::MakeView(scene,{.8f,.3f});const auto &m=view.matrix;
 auto start=std::chrono::steady_clock::now();auto actual=splat::SortIndices(scene,view);auto elapsed=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};const double radix=elapsed();
 std::vector<uint32_t> expected(scene.points.size());std::iota(expected.begin(),expected.end(),0);start=std::chrono::steady_clock::now();
 auto depth=[&](uint32_t i){const auto *p=scene.points[i].position;return m[2]*p[0]+m[6]*p[1]+m[10]*p[2];};
 std::stable_sort(expected.begin(),expected.end(),[&](auto a,auto b){return depth(a)<depth(b);});double reference=elapsed();assert(actual==expected);
 std::cout<<"PASS Rz180 covariance, translucent picking, empty-space picking, exact stable sort order; 260k radixMs="<<radix<<" referenceMs="<<reference<<"\n";
}
