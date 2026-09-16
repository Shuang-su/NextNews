#include "decode_queue.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
using namespace splat;
int main(){
    std::atomic<bool> cancel=false;std::atomic<int> live=0,peak=0,finished=0;
    auto reader=[&](uint32_t id){int n=++live;int old=peak;while(n>old&&!peak.compare_exchange_weak(old,n)){}
        std::this_thread::sleep_for(std::chrono::milliseconds(3));--live;++finished;return id*7;};
    {DecodeQueue<uint32_t> queue(cancel,{0,1,2,3,4,5},reader);
        for(uint32_t i=0;i<6;++i)assert(queue.Take(i)==i*7);}
    assert(peak==2&&finished==6&&live==0);
    finished=0;
    {DecodeQueue<uint32_t> queue(cancel,{0,1,2,3},reader);cancel=true;queue.Take(0);}
    assert(live==0&&finished==2); // pending job joined; no new read after cancel
    cancel=false;bool caught=false;
    try{DecodeQueue<uint32_t> queue(cancel,{0,1},[&](uint32_t i){if(i==0)throw std::runtime_error("corrupt input");return reader(i);});queue.Take(0);}
    catch(const std::runtime_error &){caught=true;}
    assert(caught&&live==0);
    std::cout<<"PASS two-reader bound, ordered results, cancellation join and propagated decode failure\n";
}
