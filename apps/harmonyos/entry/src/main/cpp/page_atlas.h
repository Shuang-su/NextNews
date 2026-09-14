#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>
namespace splat {
constexpr uint32_t PagePoints=256;
struct PageKey {
    uint64_t source=0;uint32_t page=0;
    bool operator<(const PageKey &b)const{return source<b.source||(source==b.source&&page<b.page);}
    bool operator==(const PageKey &b)const{return source==b.source&&page==b.page;}
};
// Render-thread owner. Planning protects BOTH the current draw set and the incoming set.
class PageAtlas {
    struct Entry {uint32_t slot;uint64_t used;bool ready;};
    std::map<PageKey,Entry> entries_;
    std::vector<uint32_t> free_;
    uint64_t clock_=0;
public:
    explicit PageAtlas(uint32_t capacity=0){Reset(capacity);}
    void Reset(uint32_t capacity){entries_.clear();free_.clear();for(uint32_t i=capacity;i>0;--i)free_.push_back(i-1);}
    bool Plan(const std::vector<PageKey>& wanted,const std::vector<PageKey>& active){
        std::set<PageKey> protectedKeys(active.begin(),active.end());protectedKeys.insert(wanted.begin(),wanted.end());
        std::set<PageKey> unique(wanted.begin(),wanted.end());size_t missing=0;
        for(const auto &key:unique)if(!entries_.count(key))++missing;
        std::vector<std::pair<uint64_t,PageKey>> victims;
        for(const auto &item:entries_)if(!protectedKeys.count(item.first))victims.push_back({item.second.used,item.first});
        if(missing>free_.size()+victims.size())return false; // no partial eviction on failure
        std::sort(victims.begin(),victims.end());
        for(size_t i=0;free_.size()<missing;++i){auto found=entries_.find(victims[i].second);free_.push_back(found->second.slot);entries_.erase(found);}
        for(const auto &key:unique){auto found=entries_.find(key);if(found==entries_.end()){auto slot=free_.back();free_.pop_back();entries_.emplace(key,Entry{slot,++clock_,false});}else found->second.used=++clock_;}
        return true;
    }
    uint32_t Slot(const PageKey &key)const{return entries_.at(key).slot;}
    bool Ready(const PageKey &key)const{return entries_.at(key).ready;}
    void MarkReady(const PageKey &key){entries_.at(key).ready=true;}
    size_t Size()const{return entries_.size();}
};
}
