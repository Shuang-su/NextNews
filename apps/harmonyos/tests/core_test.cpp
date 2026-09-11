#include "splat.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main(int argc,char **argv) {
    if(argc<3){std::cerr<<"usage: core-test valid|invalid|analytic model.ply\n";return 2;}
    const std::string mode=argv[1];
    try {
        auto scene=splat::ReadPly(argv[2]);
        if(mode=="invalid"){std::cerr<<"Accepted invalid file\n";return 1;}
        assert(!scene.points.empty());
        auto view=splat::MakeView(scene,{});auto sorted=splat::Sort(scene,view);
        float previous=-INFINITY;
        for(const auto &g:sorted){auto &m=view.matrix;float depth=m[2]*g.position[0]+m[6]*g.position[1]+m[10]*g.position[2]+m[14];assert(depth>=previous);previous=depth;}
        if(mode=="analytic") {
            const auto &g=scene.points[0];
            assert(std::abs(g.color[0]-.5f)<1e-6);
            assert(std::abs(g.color[3]-.5f)<1e-6);
            // A quarter-turn about Z swaps the anisotropic X/Y axes (scale 2,1,3).
            assert(std::abs(g.covariance[0]-1.f)<1e-5);
            assert(std::abs(g.covariance[3]-4.f)<1e-5);
            assert(std::abs(g.covariance[5]-9.f)<1e-5);
            assert(std::abs(g.covariance[1])<1e-5);
            std::atomic<bool> cancel{true};bool cancelled=false;
            try{splat::ReadPly(argv[2],&cancel);}catch(const std::exception &){cancelled=true;}
            assert(cancelled);
        }
        std::cout<<"PASS "<<mode<<" count="<<scene.points.size()<<" radius="<<scene.radius<<" bytes="<<scene.points.size()*sizeof(splat::Gaussian)<<"\n";
    }catch(const std::exception &e){
        if(mode=="invalid"){std::cout<<"PASS rejected: "<<e.what()<<"\n";return 0;}
        std::cerr<<e.what()<<"\n";return 1;
    }
}
