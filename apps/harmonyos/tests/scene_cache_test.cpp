#include "scene_cache.h"
#include <cassert>
#include <iostream>
using namespace splat;
std::shared_ptr<Scene> Make(size_t n){auto s=std::make_shared<Scene>();s->points.resize(n);return s;}
int main(){
    SceneCache<int> cache(4*sizeof(Gaussian));
    auto a=Make(2),b=Make(2),c=Make(2);
    cache.Put(1,a);cache.Put(2,b);assert(cache.Get(1)==a);
    cache.Put(3,c);assert(!cache.Get(2));assert(cache.Get(1)==a);assert(cache.Points()==4);
    assert(b->points.size()==2); // eviction cannot invalidate an active merge
    cache.Put(1,Make(1));assert(cache.Points()==3);
    cache.Put(4,Make(5));assert(!cache.Get(4));assert(cache.Points()==3);
    auto reserved=Make(1);reserved->points.reserve(10);cache.Put(9,reserved);
    assert(!cache.Get(9)); // allocated capacity, not size, must be charged
    cache.Clear();assert(cache.Points()==0&&cache.Bytes()==0);
    using Key=std::pair<std::string,std::vector<uint32_t>>;
    SceneCache<Key> subsets(4*sizeof(Gaussian));
    Key front={"chunk",{0,2}},back={"chunk",{2,2}};
    subsets.Put(front,a);subsets.Put(back,b);
    assert(subsets.Get(front)==a&&subsets.Get(back)==b);
    auto book=std::make_shared<SogTables>();
    auto encoded=[&](){auto s=std::make_shared<Scene>();s->tables=book;s->positions.resize(256);s->codes.resize(256);return s;};
    SceneCache<int> pages(sizeof(SogTables)+2*256*24);
    auto p=encoded(),q=encoded();pages.Put(1,p);pages.Put(2,q);
    assert(pages.Bytes()==sizeof(SogTables)+2*256*24&&pages.Points()==512);
    assert(pages.Get(1)==p&&pages.Get(2)==q);
    pages.Put(3,encoded());assert(!pages.Get(1)&&pages.Get(2)==q);assert(pages.Bytes()==sizeof(SogTables)+2*256*24);
    pages.Clear();assert(pages.Bytes()==0);assert(p->Count()==256&&p->tables==book);
    auto sh=std::make_shared<SogHarmonics>();sh->centroids.resize(65536);
    auto withSh=[&](){auto s=encoded();s->sogHarmonics=sh;s->shLabels.resize(256);return s;};
    const size_t pageBytes=256*32,sharedBytes=sizeof(SogTables)+sh->Bytes();
    SceneCache<int> shPages(sharedBytes+pageBytes*2);
    auto first=withSh();shPages.Put(1,first);shPages.Put(2,withSh());
    assert(shPages.Bytes()==sharedBytes+pageBytes*2);
    shPages.Put(3,withSh());assert(!shPages.Get(1)&&shPages.Get(2));
    assert(shPages.Bytes()==sharedBytes+pageBytes*2&&first->sogHarmonics==sh);
    shPages.Clear();assert(shPages.Bytes()==0);
    std::cout<<"PASS weighted LRU, touches, live eviction, replacement, limit, range-key history\n";
}
