#pragma once
#include "collision.h"
namespace viewer {
// Bounded, optional x-ray wire overlay. This does not alter collision queries,
// scene selection or rendering quality. Coordinates stay in Viewer world space.
struct DebugWire {
    std::vector<double> values;
    size_t remaining=20000,limit=1024;
    bool truncated=false;
    bool Visit(){if(!remaining||values.size()/7>=limit){truncated=true;return false;}--remaining;return true;}
    void Edge(int kind,V3 a,V3 b){if(values.size()/7>=limit){truncated=true;return;}values.insert(values.end(),{double(kind),a.x,a.y,a.z,b.x,b.y,b.z});}
    void Cube(int kind,Box b){for(int i=0;i<8;i++)for(int axis=0;axis<3;axis++)if(!(i&(1<<axis))){V3 a,c;for(int k=0;k<3;k++)a[k]=c[k]=(i&(1<<k))?b.max[k]:b.min[k];c[axis]=b.max[axis];Edge(kind,a,c);}}
};
inline bool DebugIntersects(Box a,Box b){for(int i=0;i<3;i++)if(a.max[i]<b.min[i]||a.min[i]>b.max[i])return false;return true;}
inline double DebugDistance(Box b,V3 p){V3 q;for(int i=0;i<3;i++)q[i]=std::clamp(p[i],b.min[i],b.max[i]);auto d=q-p;return d.Dot(d);}
}
