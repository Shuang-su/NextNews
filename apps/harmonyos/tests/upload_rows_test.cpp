#include "upload_rows.h"
#include <cassert>
#include <iostream>
using namespace splat;
int main(){
    const auto a=MakeUploadRows({{1,0,2048},{2,10,100}});
    assert(a.size()==3);
    const auto split=MakeUploadRows({{1,0,100},{1,100,1948},{2,10,100}});
    assert(a==split); // artificial leaf boundaries cannot force redundant uploads
    const auto changed=MakeUploadRows({{1,0,2048},{3,10,100}});
    assert(SameUploadRow(a,changed,0)&&SameUploadRow(a,changed,1)&&!SameUploadRow(a,changed,2));
    assert(!SameUploadRow(a,MakeUploadRows({{1,1,2048}}),0));
    assert(!SameUploadRow(a,MakeUploadRows({{1,0,2048},{2,10,99}}),2));
    assert(!SameUploadRow(a,{},0)); // a fresh GPU allocation has no reusable rows
    assert(MakeUploadRows({{1,0,1024}}).size()==1);
    // Simulated double buffering: A -> B -> A uses A's retained spare exactly.
    auto spare=a;auto current=changed;auto next=a;
    size_t reused=0;for(size_t i=0;i<next.size();++i)reused+=SameUploadRow(next,spare,i);
    assert(reused==next.size());
    // Same file offsets from a new stream session have distinct source identities.
    assert(!SameUploadRow(a,MakeUploadRows({{9,0,2048}}),0));
    std::cout<<"PASS exact GPU row identity, split ranges, partial rows, empty allocation, A/B/A reuse\n";
}
