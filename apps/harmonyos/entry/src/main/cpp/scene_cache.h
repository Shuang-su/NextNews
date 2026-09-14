#pragma once
#include "splat.h"
#include <map>
#include <memory>
namespace splat {
// Loader-thread-only, weighted by resident Gaussian count. Held callers remain valid on eviction.
template<class Key> class SceneCache {
    struct Entry { std::shared_ptr<Scene> scene; uint64_t used; };
    std::map<Key,Entry> entries_;
    size_t limit_, points_=0;
    uint64_t clock_=0;
public:
    explicit SceneCache(size_t limit):limit_(limit){}
    std::shared_ptr<Scene> Get(const Key &key){
        auto i=entries_.find(key);if(i==entries_.end())return {};
        i->second.used=++clock_;return i->second.scene;
    }
    void Put(const Key &key,std::shared_ptr<Scene> scene){
        auto old=entries_.find(key);if(old!=entries_.end()){points_-=old->second.scene->points.size();entries_.erase(old);}
        if(scene->points.size()>limit_)return;
        points_+=scene->points.size();entries_.emplace(key,Entry{std::move(scene),++clock_});
        while(points_>limit_){
            auto victim=entries_.begin();
            for(auto i=entries_.begin();i!=entries_.end();++i)if(i->second.used<victim->second.used)victim=i;
            points_-=victim->second.scene->points.size();entries_.erase(victim);
        }
    }
    void Clear(){entries_.clear();points_=0;}
    size_t Points() const{return points_;}
    size_t Bytes() const{size_t n=0;for(const auto &e:entries_)n+=e.second.scene->points.capacity()*sizeof(Gaussian);return n;}
};
}
