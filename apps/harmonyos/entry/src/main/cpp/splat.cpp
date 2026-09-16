#include "splat.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace splat {
namespace {
struct Property { std::string name; size_t offset; std::string type; size_t size; };
size_t SizeOf(const std::string &t) {
    if (t == "float" || t == "float32" || t == "int" || t == "uint" || t == "int32" || t == "uint32") return 4;
    if (t == "double" || t == "float64") return 8;
    if (t == "short" || t == "ushort" || t == "int16" || t == "uint16") return 2;
    if (t == "char" || t == "uchar" || t == "int8" || t == "uint8") return 1;
    throw std::runtime_error("Unsupported PLY property type: " + t);
}
float FloatLE(const char *p) {
    uint32_t u = uint8_t(p[0]) | (uint32_t(uint8_t(p[1])) << 8) |
        (uint32_t(uint8_t(p[2])) << 16) | (uint32_t(uint8_t(p[3])) << 24);
    float f; std::memcpy(&f, &u, 4); return f;
}
float Sigmoid(float x) { return x >= 0 ? 1.f / (1.f + std::exp(-x)) : std::exp(x) / (1.f + std::exp(x)); }
}

Scene ReadPly(const std::string &path, const std::atomic<bool> *cancel) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open model file");
    const auto fileSize = file.tellg();
    if (fileSize < 0 || size_t(fileSize) > MaxFileBytes) throw std::runtime_error("Model exceeds 128 MiB limit");
    file.seekg(0);
    std::string line;
    if (!std::getline(file, line) || (line != "ply" && line != "ply\r")) throw std::runtime_error("Not a PLY file");
    size_t count = 0, stride = 0, headerSize = 4;
    bool format = false, vertex = false, ended = false, seenVertex = false;
    std::vector<Property> properties;
    while (std::getline(file, line)) {
        headerSize += line.size() + 1;
        if (headerSize > 65536) throw std::runtime_error("PLY header too large");
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "end_header") { ended = true; break; }
        std::istringstream in(line); std::string key; in >> key;
        if (key == "format") {
            std::string kind, version; in >> kind >> version;
            if (kind != "binary_little_endian" || version != "1.0") throw std::runtime_error("Convert to binary little-endian 3DGS PLY first");
            format = true;
        } else if (key == "element") {
            std::string name, number; in >> name >> number;
            if (number.empty() || number.find_first_not_of("0123456789") != std::string::npos) throw std::runtime_error("Invalid element count");
            const auto n = std::stoull(number);
            if (name != "vertex") {
                if (n != 0) throw std::runtime_error("Only vertex-only Gaussian PLY is supported");
                vertex = false; continue;
            }
            if (seenVertex || n == 0 || n > MaxGaussians) throw std::runtime_error("Invalid vertex count (limit 4000000)");
            seenVertex = vertex = true; count = size_t(n);
        } else if (key == "property") {
            if (!vertex) throw std::runtime_error("Unexpected property outside vertices");
            std::string type, name; in >> type >> name;
            if (type == "list" || name.empty()) throw std::runtime_error("Unsupported list property");
            if (std::any_of(properties.begin(), properties.end(), [&](const auto &p) { return p.name == name; })) throw std::runtime_error("Duplicate PLY property");
            const auto size = SizeOf(type);
            properties.push_back({name, stride, type, size}); stride += size;
            if (stride > 4096) throw std::runtime_error("Too many PLY properties");
        }
    }
    if (!ended || !format || !count || !stride) throw std::runtime_error("Incomplete PLY header");
    const auto dataStart = file.tellg();
    if (dataStart < 0 || size_t(fileSize - dataStart) != count * stride) throw std::runtime_error("Truncated or unexpected PLY payload");
    const std::array<std::string, 14> fields = {"x", "y", "z", "f_dc_0", "f_dc_1", "f_dc_2", "opacity", "scale_0", "scale_1", "scale_2", "rot_0", "rot_1", "rot_2", "rot_3"};
    std::array<size_t, 14> offsets{};
    for (size_t i = 0; i < fields.size(); ++i) {
        auto p = std::find_if(properties.begin(), properties.end(), [&](const auto &v) { return v.name == fields[i]; });
        if (p == properties.end() || (p->type != "float" && p->type != "float32")) throw std::runtime_error("Missing float Gaussian field: " + fields[i]);
        offsets[i] = p->offset;
    }
    for (const auto &p : properties) if (p.name.rfind("f_rest_", 0) == 0) throw std::runtime_error("Convert to SH0 before importing");
    Scene scene; scene.points.reserve(count);
    std::array<float, 3> lo = {INFINITY, INFINITY, INFINITY}, hi = {-INFINITY, -INFINITY, -INFINITY};
    std::vector<char> row(stride);
    for (size_t i = 0; i < count; ++i) {
        if (cancel && cancel->load()) throw std::runtime_error("Load cancelled");
        if (!file.read(row.data(), stride)) throw std::runtime_error("Model read failed");
        std::array<float, 14> v{};
        for (size_t k = 0; k < v.size(); ++k) {
            v[k] = FloatLE(row.data() + offsets[k]);
            if (!std::isfinite(v[k])) throw std::runtime_error("Non-finite Gaussian value");
        }
        Gaussian g{};
        for (int k = 0; k < 3; ++k) {
            if (std::abs(v[k]) > 1e6f) throw std::runtime_error("Coordinate exceeds supported range");
            g.position[k] = v[k]; lo[k] = std::min(lo[k], v[k]); hi[k] = std::max(hi[k], v[k]);
            g.color[k] = std::clamp(.5f + .28209479177387814f * v[3 + k], 0.f, 1.f);
            if (v[7 + k] < -30 || v[7 + k] > 14) throw std::runtime_error("Gaussian log-scale exceeds supported range");
        }
        g.color[3] = Sigmoid(v[6]);
        double length = 0; for (int k = 10; k < 14; ++k) length += double(v[k]) * v[k];
        if (length < 1e-12) throw std::runtime_error("Zero Gaussian quaternion");
        const float inv = 1.f / std::sqrt(length);
        const float w = v[10] * inv, x = v[11] * inv, y = v[12] * inv, z = v[13] * inv;
        const float r[3][3] = {{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)}, {2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)}, {2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}};
        float c[3][3]{};
        for (int a = 0; a < 3; ++a) for (int b = 0; b < 3; ++b)
            for (int k = 0; k < 3; ++k) c[a][b] += r[a][k] * r[b][k] * std::exp(2 * v[7+k]);
        const float packed[] = {c[0][0], c[0][1], c[0][2], c[1][1], c[1][2], c[2][2]};
        std::copy(packed, packed + 6, g.covariance); scene.points.push_back(g);
    }
    double radius2 = 0;
    for (int k = 0; k < 3; ++k) { scene.center[k] = (lo[k] + hi[k]) * .5f; radius2 += double(hi[k]-lo[k]) * (hi[k]-lo[k]) * .25; }
    scene.radius = std::max(float(std::sqrt(radius2)), .001f);
    return scene;
}

void ApplyViewerTransform(Scene &scene) {
    if(scene.tables){for(auto &p:scene.positions){p[0]=-p[0];p[1]=-p[1];}scene.tables->viewerTransform=!scene.tables->viewerTransform;}
    // SuperSplat viewer's import entity: setLocalEulerAngles(0, 0, 180).
    for(auto &g:scene.points) {g.position[0]=-g.position[0];g.position[1]=-g.position[1];g.covariance[2]=-g.covariance[2];g.covariance[4]=-g.covariance[4];}
    scene.center[0]=-scene.center[0];scene.center[1]=-scene.center[1];
}
View MakeView(const Scene &scene, const Camera &camera) {
    const float y = camera.yaw, p = std::clamp(camera.pitch, -1.5f, 1.5f);
    const float right[] = {std::cos(y), 0, -std::sin(y)};
    const float up[] = {-std::sin(y)*std::sin(p), std::cos(p), -std::cos(y)*std::sin(p)};
    const float back[] = {std::sin(y)*std::cos(p), std::sin(p), std::cos(y)*std::cos(p)};
    const float distance = camera.fly > .5f ? 0.f : std::max(.01f, scene.radius * 3.f * camera.zoom);
    const float target[] = {camera.panX, camera.panY, camera.panZ};
    float eye[3]; for (int k=0;k<3;++k) eye[k]=scene.center[k]+back[k]*distance+target[k]*scene.radius;
    View v{}; auto &m = v.matrix;
    for(int k=0;k<3;++k) { m[k*4]=right[k]; m[k*4+1]=up[k]; m[k*4+2]=back[k]; }
    for(int k=0;k<3;++k) { m[12]-=right[k]*eye[k]; m[13]-=up[k]*eye[k]; m[14]-=back[k]*eye[k]; }
    m[15]=1; v.tanHalfFov=std::tan(std::clamp(camera.fov, 1.01f, 178.99f)*.00872664626f); const float centerDepth=-(m[2]*scene.center[0]+m[6]*scene.center[1]+m[10]*scene.center[2]+m[14]);
    // Keep boundary centers inside despite float rounding at the fitted far plane.
    v.farPlane=std::nextafter(std::max(centerDepth+scene.radius, .01f), INFINITY);
    v.nearPlane=std::min(1.f,std::max(centerDepth-scene.radius,v.farPlane/16384.f)); return v;
}
std::vector<float> Pick(const Scene &scene,const View &view,float x,float y,int width,int height) {
    struct Hit { float depth,alpha; size_t index; }; std::vector<Hit> hits;
    const auto &m=view.matrix;const float f=height/(2.f*view.tanHalfFov);
    const float px=(x-.5f)*width,py=(.5f-y)*height;
    for(size_t i=0;i<scene.Count();++i) {
        const auto &g=scene.At(i);float v[3]={m[12],m[13],m[14]};
        for(int r=0;r<3;++r)for(int k=0;k<3;++k)v[r]+=m[k*4+r]*g.position[k];
        const float z=-v[2];if(z<=view.nearPlane||z>=view.farPlane||g.color[3]<.004f)continue;
        const float c[3][3]={{g.covariance[0],g.covariance[1],g.covariance[2]},
            {g.covariance[1],g.covariance[3],g.covariance[4]}, {g.covariance[2],g.covariance[4],g.covariance[5]}};
        float j[2][3]{};
        const float verticalLimit=1.3f*view.tanHalfFov;const float limit=verticalLimit*width/height;
        for(int k=0;k<3;++k){j[0][k]=f/z*(m[k*4]+std::clamp(v[0]/z,-limit,limit)*m[k*4+2]);
            j[1][k]=f/z*(m[k*4+1]+std::clamp(v[1]/z,-verticalLimit,verticalLimit)*m[k*4+2]);}
        float a=.3f,b=0,d=.3f;
        for(int r=0;r<3;++r)for(int k=0;k<3;++k){a+=j[0][r]*c[r][k]*j[0][k];b+=j[0][r]*c[r][k]*j[1][k];d+=j[1][r]*c[r][k]*j[1][k];}
        const float dx=px-f*v[0]/z,dy=py-f*v[1]/z,det=a*d-b*b;
        if(det<=0)continue;
        const float power=(d*dx*dx-2*b*dx*dy+a*dy*dy)/det;
        const float alpha=std::min(.99f,g.color[3]*std::exp(-.5f*power));
        if(power<=9&&alpha>=1.f/255)hits.push_back({z,alpha,i});
    }
    std::stable_sort(hits.begin(),hits.end(),[](const Hit &a,const Hit &b){return a.depth<b.depth;});
    float transmittance=1,depth=0;bool found=false;
    for(const auto &h:hits){transmittance*=1-h.alpha;depth=h.depth;if(transmittance<=.5f){found=true;break;}}
    if(!found)return {};
    // Return the clicked ray at the composited pick depth, not the Gaussian
    // center. Large splats must not pull every click to the same screen point.
    const float cameraPoint[]={px*depth/f-m[12],py*depth/f-m[13],-depth-m[14]};
    std::vector<float> result(3);
    for(int k=0;k<3;++k)result[k]=(m[k*4]*cameraPoint[0]+m[k*4+1]*cameraPoint[1]+m[k*4+2]*cameraPoint[2]-scene.center[k])/scene.radius;
    return result;
}
std::vector<uint32_t> SortIndices(const Scene &scene, const View &view) {
    struct Item {uint32_t key,index;};
    // One independent workspace per loader/sorter thread, reused across cameras.
    struct Scratch{std::vector<Item> order,temp;};
    thread_local Scratch scratch;
    auto &order=scratch.order;auto &temp=scratch.temp;
    order.resize(scene.Count());temp.resize(scene.Count());
    size_t hist[4][256]{};
    const auto &m=view.matrix;
    for(uint32_t i=0;i<order.size();++i) {
        const auto *p=scene.Position(i);
        float depth=m[2]*p[0]+m[6]*p[1]+m[10]*p[2]; // Translation cannot change depth order.
        if(depth==0)depth=0;uint32_t bits;std::memcpy(&bits,&depth,4);
        const uint32_t key=bits^((bits&0x80000000u)?0xffffffffu:0x80000000u);
        order[i]={key,i};++hist[0][key&255];++hist[1][(key>>8)&255];++hist[2][(key>>16)&255];++hist[3][key>>24];
    }
    for(unsigned pass=0;pass<4;++pass) {
        const unsigned shift=pass*8;auto &counts=hist[pass];
        size_t offset=0;for(auto &c:counts){const auto n=c;c=offset;offset+=n;}
        for(const auto &v:order)temp[counts[(v.key>>shift)&255]++]=v;
        order.swap(temp);
    }
    std::vector<uint32_t> result;result.reserve(order.size());for(auto i:order)result.push_back(i.index);return result;
}
std::vector<Gaussian> Sort(const Scene &scene,const View &view) {
    const auto indices=SortIndices(scene,view);std::vector<Gaussian> result;result.reserve(indices.size());
    for(auto i:indices)result.push_back(scene.At(i));return result;
}
}
