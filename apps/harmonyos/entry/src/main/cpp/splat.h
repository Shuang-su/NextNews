#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace splat {
constexpr size_t MaxGaussians = 4000000;
constexpr size_t MaxFileBytes = 128 * 1024 * 1024;
struct Gaussian {
    float position[3];
    float color[4];
    float covariance[6]; // xx, xy, xz, yy, yz, zz
};
struct Scene;
struct SceneRange { std::shared_ptr<const Scene> source; uint32_t offset,count,logical; };
struct Scene {
    std::vector<Gaussian> points;
    bool paged=false;
    std::vector<std::array<float,3>> positions;
    std::vector<uint32_t> addresses;
    std::vector<SceneRange> ranges;
    size_t Count()const{return paged?positions.size():points.size();}
    const float *Position(size_t i)const{return paged?positions[i].data():points[i].position;}
    const Gaussian &At(size_t i)const{
        if(!paged)return points.at(i);
        auto r=std::upper_bound(ranges.begin(),ranges.end(),i,[](size_t value,const SceneRange &range){return value<range.logical;});
        if(r==ranges.begin())throw std::out_of_range("Invalid page range");--r;
        return r->source->points.at(r->offset+i-r->logical);
    }
    std::array<float, 3> center{};
    float radius = 1;
};
Scene ReadSog(const std::string &path, const std::atomic<bool> *cancel = nullptr);
Scene ReadModel(const std::string &path, const std::atomic<bool> *cancel = nullptr);
void ApplyViewerTransform(Scene &scene);
Scene ReadPly(const std::string &path, const std::atomic<bool> *cancel = nullptr);
struct Camera {
    float yaw = 0, pitch = 0, zoom = 1, panX = 0, panY = 0, panZ = 0, fly = 0, fov = 45;
};
struct View {
    std::array<float, 16> matrix;
    float nearPlane, farPlane, tanHalfFov = .41421356f;
};
View MakeView(const Scene &scene, const Camera &camera);
std::vector<float> Pick(const Scene &scene, const View &view, float x, float y, int width, int height);
std::vector<uint32_t> SortIndices(const Scene &scene, const View &view);
std::vector<Gaussian> Sort(const Scene &scene, const View &view);
}
