#pragma once
#include "collision.h"
#include <map>
#include <set>
namespace viewer {
// Immutable query snapshots hold shared references while the loader/LRU changes.
class Atlas final:public Collision {
public:
    std::vector<std::shared_ptr<const Voxel>> tiles;
    std::optional<V3> Ray(V3 o,V3 d,double distance)const override {
        std::optional<V3> best;double length=distance;
        for(const auto &tile:tiles){auto hit=tile->Ray(o,d,length);if(hit){length=(*hit-o).Length();best=hit;}}
        return best;
    }
    bool Deepest(V3 p,double half,double radius,V3 &push)const override {
        double best=0;for(const auto &tile:tiles){V3 value;if(tile->Deepest(p,half,radius,value)&&value.Dot(value)>best){best=value.Dot(value);push=value;}}
        return best>0;
    }
    bool Free(V3 p)const override {
        bool known=false;
        for(const auto &tile:tiles)if(tile->Bounds().Contains(p)){known=true;if(!tile->Free(p))return false;}
        return known;
    }
    bool Known(Box area)const override {
        std::vector<Box> gaps{area};
        for(const auto &tile:tiles){
            if(!tile->Available())continue;
            std::vector<Box> next;const auto cover=tile->Bounds();
            for(auto gap:gaps){
                Box overlap;bool intersects=true;
                for(int i=0;i<3;i++){overlap.min[i]=std::max(gap.min[i],cover.min[i]);overlap.max[i]=std::min(gap.max[i],cover.max[i]);if(overlap.min[i]>=overlap.max[i])intersects=false;}
                if(!intersects){next.push_back(gap);continue;}
                for(int i=0;i<3;i++){
                    if(gap.min[i]<overlap.min[i]){Box slice=gap;slice.max[i]=overlap.min[i];next.push_back(slice);gap.min[i]=overlap.min[i];}
                    if(gap.max[i]>overlap.max[i]){Box slice=gap;slice.min[i]=overlap.max[i];next.push_back(slice);gap.max[i]=overlap.max[i];}
                }
            }
            gaps=std::move(next);if(gaps.empty())return true;
        }return false;
    }
    double Resolution()const override{double result=10;for(const auto &tile:tiles)result=std::min(result,tile->Resolution());return result;}
};
class CollisionCache {
    struct Entry {std::shared_ptr<const Voxel> tile;uint64_t used;};
    std::map<std::string,Entry> entries_;
    std::set<std::string> selected_;
    size_t budget_,bytes_=0;uint64_t tick_=0;
public:
    explicit CollisionCache(size_t budget=128*1024*1024):budget_(budget){}
    size_t Bytes()const{return bytes_;}
    size_t Count()const{return entries_.size();}
    bool Has(const std::string &id)const{return entries_.count(id)!=0;}
    void Select(std::set<std::string> ids){selected_=std::move(ids);for(const auto &id:selected_){auto it=entries_.find(id);if(it!=entries_.end())it->second.used=++tick_;}}
    bool Insert(const std::string &id,std::shared_ptr<const Voxel> tile){
        if(Has(id))return true;
        const size_t required=tile->Bytes();if(required>budget_)return false;
        while(bytes_+required>budget_){
            auto oldest=entries_.end();
            for(auto it=entries_.begin();it!=entries_.end();++it)if(!selected_.count(it->first)&&it->second.tile.use_count()==1&&(oldest==entries_.end()||it->second.used<oldest->second.used))oldest=it;
            if(oldest==entries_.end())return false;
            bytes_-=oldest->second.tile->Bytes();entries_.erase(oldest);
        }
        bytes_+=required;entries_.emplace(id,Entry{std::move(tile),++tick_});return true;
    }
    Atlas Snapshot()const {Atlas a;for(const auto &id:selected_){auto it=entries_.find(id);if(it!=entries_.end())a.tiles.push_back(it->second.tile);}return a;}
    std::vector<std::string> Ids()const{std::vector<std::string> ids;for(const auto &e:entries_)ids.push_back(e.first);return ids;}
};
}
