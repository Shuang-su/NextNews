#pragma once
#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <vector>
namespace splat {
// Owner-thread scheduling; workers only produce values. Futures are destroyed
// before the reader and join before the owner's cancellation token is reused.
template<class T> class DecodeQueue {
    const std::atomic<bool> &cancel_;
    std::vector<uint32_t> inputs_;
    std::function<T(uint32_t)> read_;
    size_t next_=0;
    std::map<uint32_t,std::future<T>> jobs_;
    void Fill(){
        while(!cancel_&&jobs_.size()<2&&next_<inputs_.size()){
            const auto id=inputs_[next_++];
            jobs_.emplace(id,std::async(std::launch::async,[this,id](){return read_(id);}));
        }
    }
public:
    DecodeQueue(const std::atomic<bool> &cancel,std::vector<uint32_t> inputs,std::function<T(uint32_t)> read)
        :cancel_(cancel),inputs_(std::move(inputs)),read_(std::move(read)){Fill();}
    DecodeQueue(const DecodeQueue&)=delete;
    DecodeQueue& operator=(const DecodeQueue&)=delete;
    bool Contains(uint32_t id)const{return jobs_.count(id)!=0;}
    T Take(uint32_t id){auto found=jobs_.find(id);if(found==jobs_.end())throw std::runtime_error("Source decode was not queued");
        T result=found->second.get();jobs_.erase(found);Fill();return result;}
};
}
