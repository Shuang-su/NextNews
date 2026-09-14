#include "lod_selection.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cassert>
using namespace splat;
int main(int argc,char **argv){
    if(argc==2){std::ifstream file(argv[1]);nlohmann::json m;file>>m;LodTree tree;tree.bounds=m["bounds"].get<std::array<double,4>>();tree.levels=m["levels"];
        for(auto &leaf:m["leaves"]){const auto b=leaf["bounds"].get<std::array<double,4>>();std::vector<double> box=leaf.contains("aabb")?leaf["aabb"].get<std::vector<double>>():std::vector<double>{b[0]-b[3],b[1]-b[3],b[2]-b[3],b[0]+b[3],b[1]+b[3],b[2]+b[3]};tree.boxes.insert(tree.boxes.end(),box.begin(),box.end());for(auto &l:leaf["lods"])for(auto &v:l)tree.lods.push_back(v);}
        nlohmann::json result=nlohmann::json::array();for(const auto yaw:{0.,.846,-.65,3.14159265358979323846})for(const auto budget:{2000000u,4000000u,8000000u})result.push_back(tree.Select({yaw,.056,1,.412,-.139,.446,1},budget,75,.5032405642394204));std::cout<<result.dump();return 0;
    }
    LodTree tree;tree.bounds={0,0,0,10};tree.levels=2;tree.boxes={-.2,-.2,-5.2,.2,.2,-4.8,-.2,-.2,4.8,.2,.2,5.2};tree.lods={0,0,900,0,900,10,0,1000,900,0,1900,10};
    assert((tree.Select({0,0,1,0,0,0,1},1000,45,1)==std::vector<uint32_t>{0,1}));
    assert((tree.Select({3.14159265358979323846,0,1,0,0,0,1},1000,45,1)==std::vector<uint32_t>{1,0}));
    assert((tree.Select({0,0,1,0,0,0,1},20,45,1)==std::vector<uint32_t>{1,1}));
    std::cout<<"PASS native LOD front/back, budget and complete coarse coverage\n";
}
