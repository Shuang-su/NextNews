#include "collision/mesh.h"
#include "third_party/nlohmann/json.hpp"
#include <iostream>
using namespace viewer;using J=nlohmann::json;
V3 V(const J &j){return {j[0],j[1],j[2]};}J A(V3 v){return J::array({v.x,v.y,v.z});}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    try{
        auto c=Mesh::Load(argv[1]);J request;std::cin>>request;J result=J::array();
        for(const auto &q:request){
            std::string op=q["op"];
            if(op=="ray"){auto h=c->Ray(V(q["p"]),V(q["d"]),q["distance"]);result.push_back(h?A(*h):J(nullptr));}
            else if(op=="free")result.push_back(c->Free(V(q["p"])));
            else if(op=="spawn"){auto h=FindSpawn(*c,V(q["p"]));result.push_back(h?A(*h):J(nullptr));}
            else if(op=="walk"){
                Walker w;if(!w.Enter(*c,V(q["p"])))throw std::runtime_error("No spawn");double peak=w.Eye().y;
                for(int i=0;i<q["frames"];i++){auto r=w.Update(*c,1.0/60,q.value("yaw",0.0),q.value("right",0.0)/60,q.value("forward",0.0)/60,q.value("jump",false));peak=std::max(peak,r.eye.y);}
                result.push_back({{"eye",A(w.Eye())},{"peak",peak}});
            }else{V3 push;bool hit=c->Capsule(V(q["p"]),op=="sphere"?0:q["half"].get<double>(),q["radius"],push);result.push_back(hit?A(push):J(nullptr));}
        }
        std::cout<<result.dump()<<"\n";
    }catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}
}
