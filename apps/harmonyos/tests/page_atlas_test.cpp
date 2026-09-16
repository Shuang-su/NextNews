#include "page_atlas.h"
#include <cassert>
#include <iostream>
using namespace splat;
int main(){
    PageAtlas atlas(3);PageKey a{1,0},b{1,1},c{2,0},d{2,1};
    assert(atlas.Plan({a,b},{}));auto sa=atlas.Slot(a),sb=atlas.Slot(b);atlas.MarkReady(a);atlas.MarkReady(b);
    assert(atlas.Plan({b,c},{a,b}));assert(atlas.Slot(a)==sa&&atlas.Slot(b)==sb);atlas.MarkReady(c);
    assert(!atlas.Plan({d},{a,b,c}));assert(atlas.Size()==3&&atlas.Ready(a));
    assert(atlas.Plan({b,d},{b,c}));assert(atlas.Slot(b)==sb);assert(atlas.Slot(d)==sa&&!atlas.Ready(d));
    atlas.MarkReady(d);assert(atlas.Plan({d,b},{b,d}));assert(atlas.Ready(d)&&atlas.Ready(b));
    atlas.Reset(3);assert(atlas.Plan({a},{}));assert(!atlas.Ready(a));
    std::cout<<"PASS stable slots, dual-set pins, transactional exhaustion, LRU, context reset\n";
}
