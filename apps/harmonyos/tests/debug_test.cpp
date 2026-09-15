#include "collision/debug.h"
#include "collision/mesh.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace viewer;
int main(){
    Box all{{-10,-10,-10},{10,10,10}};
    Voxel one({{0,0,0},{1,1,1}},.25,0,false,{0},{1,0});
    DebugWire a;one.Debug(all,a);assert(a.values.size()==12*7&&!a.truncated);
    for(size_t i=0;i<a.values.size();i+=7)for(int j=1;j<7;j++)assert(a.values[i+j]>=0&&a.values[i+j]<=.25);
    Voxel legacy({{0,0,0},{1,1,1}},.25,0,true,{0},{1,0});DebugWire b;legacy.Debug(all,b);assert(b.values.size()==a.values.size());
    std::set<std::array<double,3>> expected,actual;
    for(size_t i=0;i<a.values.size();i+=7)for(int j:{1,4}){expected.insert({-a.values[i+j],-a.values[i+j+1],a.values[i+j+2]});actual.insert({b.values[i+j],b.values[i+j+1],b.values[i+j+2]});}assert(actual==expected);
    DebugWire away;one.Debug({{2,2,2},{3,3,3}},away);assert(away.values.empty());
    Voxel full({{0,0,0},{1,1,1}},.25,0,false,{0xff000000},{});DebugWire c;full.Debug(all,c);assert(c.values.size()==84);assert(c.values[4]==1);
    DebugWire bounded;bounded.limit=3;one.Debug(all,bounded);assert(bounded.values.size()==21&&bounded.truncated);
    DebugWire budget;budget.remaining=1;one.Debug(all,budget);assert(budget.values.empty()&&budget.truncated);
    // A leaf at a high level repeats its 4-cell pattern, without leaking into
    // a neighbouring octree child at an inclusive upper grid boundary.
    Voxel nested({{0,0,0},{2,2,2}},.25,1,false,{0x01000001,0},{1,0});DebugWire n;nested.Debug(all,n);assert(n.values.size()==84);
    Voxel repeat({{0,0,0},{2,2,2}},.25,1,false,{0},{1,0});DebugWire r;repeat.Debug(all,r);assert(r.values.size()==8*84);
    Mesh mesh({{0,0,0},{1,0,0},{0,1,0}},{0,1,2});DebugWire m;mesh.Debug(all,m);assert(m.values.size()==21);for(size_t i=0;i<m.values.size();i+=7)assert(m.values[i]==1);
    DebugWire far;mesh.Debug({{2,2,2},{3,3,3}},far);assert(far.values.empty());
    std::cout<<"PASS collision wire: exact cell/legacy/mesh coordinates, outside regions, early leaves, child boundaries, primitive and work limits\n";
}
