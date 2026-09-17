#include "sh_pages.h"
#include <cassert>
#include <iostream>
using namespace splat;
int main(){
 for(uint32_t degree=0;degree<=3;degree++)for(uint32_t book:{0u,1u,2047u})for(uint32_t label:{0u,63u,64u,65535u}){
  auto value=SogReference(book,label,degree);assert((value&2047u)==book);assert(((value>>11)&65535u)==label);assert(((value>>27)&3u)==degree);
 }
 bool bad=false;try{SogReference(2048,0,0);}catch(...){bad=true;}assert(bad);
 for(uint32_t degree=1;degree<=3;degree++){
  uint32_t n=(degree+1)*(degree+1)-1;
  for(uint32_t label:{0u,63u,64u,1092u,65535u})for(uint32_t c=0;c<n;c++){
   const auto a=ShLinear(label,degree,c);
   assert(a==(label/64)*(64*n)+(label%64)*n+c);
   assert(a/ShPageTexels<ShSourcePages);
  }
 }
 PageAtlas atlas(3);std::vector<PageKey> old={{1,0},{1,1}},next={{1,1},{2,0}};
 assert(atlas.Plan(old,{}));for(auto k:old)atlas.MarkReady(k);const auto stable=atlas.Slot(old[1]);
 assert(atlas.Plan(next,old));assert(atlas.Slot(old[1])==stable);atlas.MarkReady(next[1]);
 assert(atlas.Plan(next,next));for(auto k:next)assert(atlas.Ready(k)); // warm selection needs no upload
 assert(!atlas.Plan({{3,0},{3,1}},next));for(auto k:next)assert(atlas.Ready(k)); // preserves active contents
 std::cout<<"PASS SH packed source/label/degree, centroid page boundaries and protected warm slots\n";
}
