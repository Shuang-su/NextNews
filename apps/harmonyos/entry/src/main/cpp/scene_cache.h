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
    std::list<Key> lru_;
    size_t limit_,bytes_=0,points_=0;
    void Erase(typename std::map<Key,Entry>::iterator i){
        bytes_-=i->second.bytes;points_-=i->second.scene->points.size();lru_.erase(i->second.age);entries_.erase(i);
    }
public:
    explicit SceneCache(size_t byteLimit):limit_(byteLimit){}
    std::shared_ptr<Scene> Get(const Key &key){
        auto i=entries_.find(key);if(i==entries_.end())return {};
        lru_.splice(lru_.end(),lru_,i->second.age);return i->second.scene;
    }
    void Put(const Key &key,std::shared_ptr<Scene> scene){
        auto old=entries_.find(key);if(old!=entries_.end())Erase(old);
        const size_t bytes=scene->points.capacity()*sizeof(Gaussian)+scene->positions.capacity()*sizeof(std::array<float,3>)+scene->addresses.capacity()*sizeof(uint32_t)+scene->ranges.capacity()*sizeof(SceneRange);
        if(bytes>limit_)return;
        while(bytes_+bytes>limit_&&!lru_.empty())Erase(entries_.find(lru_.front()));
        bytes_+=bytes;points_+=scene->points.size();lru_.push_back(key);
        entries_.emplace(key,Entry{std::move(scene),bytes,std::prev(lru_.end())});
    }
    void Clear(){entries_.clear();lru_.clear();bytes_=points_=0;}
    size_t Points()const{return points_;}
    size_t Bytes()const{return bytes_;}
};
}
