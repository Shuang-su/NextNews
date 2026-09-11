#pragma once
#include "splat.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace splat {
struct Status {
    std::string state = "waiting", message = "Waiting for render surface", graphics;
    size_t count = 0, bytes = 0;
    double loadMs = 0, sortMs = 0, frameMs = 0, fps = 0;
};
class Renderer {
public:
    static Renderer &Get();
    ~Renderer();
    void Start(void *window, int width, int height);
    void Stop();
    void Resize(int width, int height);
    void Load(std::string path);
    void SetCamera(Camera camera);
    void SetChunks(std::vector<std::string> paths, std::array<float,4> bounds);
    void SetActive(bool active);
    Status GetStatus();
private:
    void Loop(void *window);
    void InitGL(void *window);
    void DestroyGL();
    void Draw(const View &view, int width, int height);
    std::mutex mutex_;
    std::condition_variable changed_;
    std::thread worker_;
    std::atomic<bool> cancel_{false};
    bool stop_ = false, dirty_ = true, active_ = true;
    int width_ = 1, height_ = 1;
    std::string pendingPath_;
    std::string requestedPath_;
    std::vector<std::string> chunkPaths_;
    std::array<float,4> chunkBounds_{};
    bool chunksDirty_ = false;
    Camera camera_;
    Status status_;
    Scene scene_;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
    GLuint program_ = 0, vao_ = 0, buffer_ = 0;
    void *window_ = nullptr;
    int bufferWidth_ = 0, bufferHeight_ = 0;
};
}
