#include "intro.h"
#include <cassert>
#include <cmath>
int main() {
    splat::Intro intro;
    assert(intro.Frame(0)==1);
    intro.Request(true,true);
    assert(!intro.Running() && intro.Frame(50)==1); // wait for GPU commit
    intro.Commit(); assert(intro.Frame(100)==0);
    assert(std::abs(intro.Frame(100.6)-.5f)<.0001f);
    intro.Commit(); // LOD refinement cannot restart opening
    assert(std::abs(intro.Frame(100.6)-.5f)<.0001f);
    intro.Pause(); assert(std::abs(intro.Frame(500)-.5f)<.0001f);
    assert(intro.Frame(501)==1 && !intro.Running());
    intro.Request(true,false);assert(intro.Frame(600)==0);
    intro.Request(false,false);assert(intro.Frame(601)==1);
    intro.Request(true,true);intro.Request(false,false);intro.Commit();assert(intro.Frame(700)==1);
    intro.Request(true,true);intro.Request(true,true);intro.Commit();assert(intro.Frame(800)==0);
}
