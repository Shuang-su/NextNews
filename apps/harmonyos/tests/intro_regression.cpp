#include "intro.h"
#include <cassert>
#include <cmath>
int main() {
    assert(std::abs(splat::FarthestCorner({10,0,0},{-1,-2,-3,1,2,3})-std::sqrt(134.f))<1e-5);
    const auto ordinary=splat::RevealMotion::For(100,0),character=splat::RevealMotion::For(100,1),mega=splat::RevealMotion::For(100,2);
    assert(std::abs(character.dotSize-.00063f)<1e-8);
    assert(std::abs(ordinary.dotSize-.0066f)<1e-7);
    assert(std::abs(mega.dotSize-.022f)<1e-7);
    assert(mega.Duration(100)<ordinary.Duration(100));
    assert(mega.delay>1 && mega.oscillation==.16f);
    splat::Intro intro;
    assert(intro.Frame(0)==1);
    intro.Request(true,true);assert(!intro.Running() && intro.Frame(50)==1);
    intro.Commit(20);assert(intro.Frame(10)==0);assert(intro.Frame(90)==0);assert(!intro.Running());
    intro.BeginVisible(20);assert(intro.Running());assert(intro.Frame(100)==0);
    for(int i=1;i<=30;i++)intro.Frame(100+i/30.0);
    const float before=intro.Frame(101);assert(before>0 && before<1);
    intro.Commit(200);assert(intro.Frame(101)==before); // LOD must not change duration
    intro.Pause();assert(intro.Frame(500)==before);
    intro.Frame(600);assert(intro.Seconds()<1.04); // long frames cannot skip the wave
    for(int i=1;i<1000;i++)intro.Frame(600+i/30.0);
    assert(!intro.Running());
    intro.Request(true,false);assert(intro.Frame(700)==0);
    intro.Request(false,false);assert(intro.Frame(701)==1);
    intro.Request(true,true);intro.Request(false,false);intro.Commit();assert(intro.Frame(800)==1);
    assert(splat::Intro::Duration(100)>splat::Intro::Duration(20));
}
