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
        assert(scene.hasWorldBox);
        for(const auto &point:scene.points)for(int k=0;k<3;++k){assert(point.position[k]>=scene.worldBox[k]);assert(point.position[k]<=scene.worldBox[k+3]);}
        auto transformed=scene;splat::ApplyViewerTransform(transformed);
        for(const auto &point:transformed.points)for(int k=0;k<3;++k){assert(point.position[k]>=transformed.worldBox[k]);assert(point.position[k]<=transformed.worldBox[k+3]);}
        auto view=splat::MakeView(scene,{});auto sorted=splat::Sort(scene,view);
        float previous=-INFINITY;
        for(const auto &g:sorted){auto &m=view.matrix;float depth=m[2]*g.position[0]+m[6]*g.position[1]+m[10]*g.position[2]+m[14];assert(depth>=previous);previous=depth;}
        if(mode=="analytic") {
            // A panned target stays at the orbit center after rotation (world-space target).
            splat::Camera orbit{.7f, .4f, 1.2f, .2f, -.3f, .6f};
            const auto orbitView = splat::MakeView(scene, orbit);
            const float target[] = {scene.center[0]+orbit.panX*scene.radius,
                scene.center[1]+orbit.panY*scene.radius, scene.center[2]+orbit.panZ*scene.radius};
            for (int row=0; row<3; ++row) {
                float value=orbitView.matrix[12+row];
                for (int k=0;k<3;++k) value+=orbitView.matrix[k*4+row]*target[k];
                // The one-point fixture has a 0.001 radius; the Viewer keeps a
                // 0.01 world-unit minimum orbit distance (not a radius-relative cap).
                const float expected=row==2 ? -std::max(.01f,scene.radius*3.f*orbit.zoom) : 0.f;
                assert(std::abs(value-expected)<1e-4f);
            }
            orbit.fly=1;
            const auto flyView=splat::MakeView(scene,orbit);
            for(int row=0;row<3;++row){float v=flyView.matrix[12+row];
                for(int k=0;k<3;++k)v+=flyView.matrix[k*4+row]*target[k];
                assert(std::abs(v)<1e-4f);}
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
