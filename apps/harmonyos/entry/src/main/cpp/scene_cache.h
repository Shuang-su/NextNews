#pragma once
#include "splat.h"
#include <list>
#include <map>
#include <memory>
namespace splat {
// Loader-thread-only. Charges allocated vector bytes, not a point-count estimate.
// The LRU list makes eviction O(log n), including large source-page caches.
// Shared owners keep active draws and pending selections valid after eviction.
template<class Key> class SceneCache {
    struct Entry { std::shared_ptr<Scene> scene; size_t bytes; typename std::list<Key>::iterator age; };
    std::map<Key,Entry> entries_;
    std::map<const SogTables*,size_t> tableRefs_;
    std::map<const SogHarmonics*,size_t> shRefs_;
    std::list<Key> lru_;
    size_t limit_,bytes_=0,points_=0;
    void Erase(typename std::map<Key,Entry>::iterator i){
        if(i->second.scene->tables){auto t=tableRefs_.find(i->second.scene->tables.get());if(--t->second==0){bytes_-=sizeof(SogTables);tableRefs_.erase(t);}}
        if(i->second.scene->sogHarmonics){auto h=shRefs_.find(i->second.scene->sogHarmonics.get());if(--h->second==0){bytes_-=h->first->Bytes();shRefs_.erase(h);}}
        bytes_-=i->second.bytes;points_-=i->second.scene->Count();lru_.erase(i->second.age);entries_.erase(i);
    }
public:
    explicit SceneCache(size_t byteLimit):limit_(byteLimit){}
    bool Contains(const Key &key)const{return entries_.count(key)!=0;}
    std::shared_ptr<Scene> Get(const Key &key){
        auto i=entries_.find(key);if(i==entries_.end())return {};
        lru_.splice(lru_.end(),lru_,i->second.age);return i->second.scene;
    }
    void Put(const Key &key,std::shared_ptr<Scene> scene){
        auto old=entries_.find(key);if(old!=entries_.end())Erase(old);
        const size_t bytes=scene->shLabels.capacity()*sizeof(std::array<uint32_t,2>)+scene->harmonics.capacity()*sizeof(std::array<float,48>)+scene->points.capacity()*sizeof(Gaussian)+scene->positions.capacity()*sizeof(std::array<float,3>)+scene->addresses.capacity()*sizeof(uint32_t)+scene->ranges.capacity()*sizeof(SceneRange)+scene->codes.capacity()*sizeof(SogCodes);
        const auto tableExtra=[&](){return scene->tables&&!tableRefs_.count(scene->tables.get())?sizeof(SogTables):0;};
        const auto shExtra=[&](){return scene->sogHarmonics&&!shRefs_.count(scene->sogHarmonics.get())?scene->sogHarmonics->Bytes():0;};
        const auto extra=[&](){return tableExtra()+shExtra();};
        if(bytes+(scene->tables?sizeof(SogTables):0)+(scene->sogHarmonics?scene->sogHarmonics->Bytes():0)>limit_)return;
        while(bytes_+bytes+extra()>limit_&&!lru_.empty())Erase(entries_.find(lru_.front()));
        if(scene->tables){bytes_+=tableExtra();++tableRefs_[scene->tables.get()];}
        if(scene->sogHarmonics){bytes_+=shExtra();++shRefs_[scene->sogHarmonics.get()];}
        bytes_+=bytes;points_+=scene->Count();lru_.push_back(key);
        entries_.emplace(key,Entry{std::move(scene),bytes,std::prev(lru_.end())});
    }
    void Clear(){entries_.clear();lru_.clear();tableRefs_.clear();shRefs_.clear();bytes_=points_=0;}
    size_t Points()const{return points_;}
    size_t Bytes()const{return bytes_;}
};
}
