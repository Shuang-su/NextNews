#include "collision/collision.h"
#include "collision/atlas.h"
#include "third_party/nlohmann/json.hpp"
#include <iostream>
#include <cassert>
using namespace viewer;using J=nlohmann::json;
V3 V(const J &j){return {j[0],j[1],j[2]};}J A(V3 v){return J::array({v.x,v.y,v.z});}
int main(int argc,char **argv){
    if(argc==1){
        auto make=[](double x){return std::make_shared<Voxel>(Box{{x,0,0},{x+.4,.4,.4}},.1,0,false,std::vector<uint32_t>{0},std::vector<uint32_t>{0,0});};
        CollisionCache cache(24);assert(cache.Insert("a",make(0)));assert(cache.Insert("b",make(.4)));cache.Select({"a","b"});
        {auto snapshot=cache.Snapshot();assert(snapshot.Known({{.1,.1,.1},{.7,.3,.3}}));assert(!snapshot.Known({{.1,.1,.1},{.9,.3,.3}}));cache.Select({"b"});assert(!cache.Insert("c",make(.8)));}
        assert(cache.Insert("c",make(.8)));assert(!cache.Has("a")&&cache.Has("b")&&cache.Has("c"));
        Atlas gaps;gaps.tiles={make(0),make(.8)};assert(!gaps.Known({{.1,.1,.1},{1.1,.3,.3}}));
        auto empty=std::make_shared<Voxel>(Box{{0,0,0},{.4,.4,.4}},.1,0,false,std::vector<uint32_t>{},std::vector<uint32_t>{});Atlas unknown;unknown.tiles={empty};assert(!unknown.Known({{.1,.1,.1},{.3,.3,.3}}));
        std::cout<<"PASS atlas union, missing gap, empty data, pinning and byte LRU\n";return 0;
    }
    if(argc!=3&&argc!=4)return 2;
    try {
        auto c=Voxel::Load(argv[1],argv[2],argc>3?std::stoi(argv[3]):-1);J request;std::cin>>request;J result=J::array();
        for(const auto &q:request){
            std::string op=q["op"];
            if(op=="ray"){auto hit=c->Ray(V(q["p"]),V(q["d"]),q["distance"]);result.push_back(hit?A(*hit):J(nullptr));}
            else if(op=="capsule"||op=="sphere"){V3 push;bool hit=c->Capsule(V(q["p"]),op=="sphere"?0:q["half"].get<double>(),q["radius"],push);result.push_back(hit?A(push):J(nullptr));}
            else if(op=="free")result.push_back(c->Free(V(q["p"])));
            else if(op=="known")result.push_back(c->Known({V(q["min"]),V(q["max"])}));
            else if(op=="spawn"){auto point=FindSpawn(*c,V(q["p"]));result.push_back(point?A(*point):J(nullptr));}
            else if(op=="walk"){
                Walker w;assert(w.Enter(*c,V(q["p"])));auto spawn=w.Eye();double high=spawn.y;bool anyBlocked=false;
                for(int i=0;i<q["frames"];i++){auto r=w.Update(*c,1.0/60,q.value("yaw",0.0),q.value("right",0.0)/60,q.value("forward",0.0)/60,q.value("jump",false));high=std::max(high,r.eye.y);anyBlocked|=r.blocked;}
                result.push_back({{"eye",A(w.Eye())},{"peak",high},{"blocked",anyBlocked}});w.Reset();assert((w.Eye()-spawn).Length()<1e-8);
            }else throw std::runtime_error("Unknown operation");
        }
        std::cout<<result.dump()<<"\n";
    }catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}
}
