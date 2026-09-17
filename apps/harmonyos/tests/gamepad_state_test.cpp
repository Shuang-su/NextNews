#include "gamepad_state.h"
#include <cassert>
#include <limits>
int main(){
 splat::GamepadState s;s.Axis("a",false,1,-.5);s.Axis("b",false,1,.5);s.Axis("a",true,.25,-.25);
 auto v=s.Read();assert(v[0]==2&&v[1]==0&&v[2]==.25&&v[3]==-.25);
 s.Remove("b");assert(s.Read()[0]==1);s.Axis("a",false,0,0);assert(s.Read()[2]==.25);
 s.Axis("a",true,std::numeric_limits<double>::quiet_NaN(),0);assert(s.Read()[2]==.25);
 s.Remove("a");assert((s.Read()==std::array<double,4>{}));
 s.Axis("a",false,2,-2);assert(s.Read()[0]==1&&s.Read()[1]==-1);
 s.Clear();assert((s.Read()==std::array<double,4>{}));
}
