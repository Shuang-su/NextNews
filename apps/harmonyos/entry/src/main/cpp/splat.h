#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace splat {
constexpr size_t MaxGaussians = 300000;
constexpr size_t MaxFileBytes = 128 * 1024 * 1024;
struct Gaussian {
    float position[3];
    float color[4];
    float covariance[6]; // xx, xy, xz, yy, yz, zz
};
struct Scene {
    std::vector<Gaussian> points;
    std::array<float, 3> center{};
    float radius = 1;
};
Scene ReadPly(const std::string &path, const std::atomic<bool> *cancel = nullptr);
struct Camera {
    float yaw = 0, pitch = 0, zoom = 1, panX = 0, panY = 0, panZ = 0, fly = 0;
};
struct View {
    std::array<float, 16> matrix;
    float nearPlane, farPlane;
};
View MakeView(const Scene &scene, const Camera &camera);
std::vector<Gaussian> Sort(const Scene &scene, const View &view);
}
