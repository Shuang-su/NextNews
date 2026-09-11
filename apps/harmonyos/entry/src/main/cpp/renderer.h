#pragma once
#include "splat.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <condition_variable>
#include <mutex>
#include <map>
#include <memory>
#include <thread>

namespace splat {
struct Status {
    std::string state = "waiting", message = "Waiting for render surface", graphics;
    size_t count = 0, bytes = 0, frames = 0;
    int width = 1, height = 1;
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
    void SetChunks(std::vector<std::string> paths, std::array<float,4> bounds, std::vector<uint32_t> ranges = {});
    void SetActive(bool active);
    Status GetStatus();
    std::vector<float> Pick(float x,float y);
private:
    void Loop(void *window);
    void SortLoop();
    void InitGL(void *window);
    void DestroyGL();
    void Draw(const View &view, int width, int height);
    std::mutex mutex_;
    std::condition_variable changed_;
    std::thread worker_, sorter_;
    std::condition_variable sortChanged_;
    std::shared_ptr<Scene> sortScene_, sortedScene_;
    View sortView_{};
    std::vector<uint32_t> sortedIndices_;
    bool sortPending_ = false, sortReady_ = false;
    double completedSortMs_ = 0;
    std::atomic<bool> cancel_{false};
    bool stop_ = false, dirty_ = true, active_ = true;
    int width_ = 1, height_ = 1;
    std::string pendingPath_;
    std::string requestedPath_;
    std::vector<std::string> chunkPaths_;
    std::array<float,4> chunkBounds_{};
    std::vector<uint32_t> chunkRanges_;
    std::map<std::string,std::shared_ptr<Scene>> decoded_;
    std::map<std::string,std::pair<std::vector<uint32_t>,std::shared_ptr<Scene>>> selected_;
    bool chunksDirty_ = false;
    Camera camera_;
    Status status_;
    std::shared_ptr<Scene> scene_ = std::make_shared<Scene>();
    bool uploadDirty_ = true;
    std::array<float,3> sortDirection_{};
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
    GLuint program_ = 0, vao_ = 0, buffer_ = 0, dataTexture_ = 0;
    void *window_ = nullptr;
    int bufferWidth_ = 0, bufferHeight_ = 0;
};
}
