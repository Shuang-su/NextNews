#pragma once
#include "splat.h"
#include "upload_rows.h"
#include "scene_cache.h"
#include "page_atlas.h"
#include "hotspots.h"
#include "intro.h"
#include "post_process.h"
#include "skybox.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <condition_variable>
#include <mutex>
#include <map>
#include <memory>
#include <thread>
#include <functional>

namespace splat {
struct Status {
    std::string state = "waiting", message = "Waiting for render surface", graphics;
    size_t count = 0, bytes = 0, frames = 0;
    int width = 1, height = 1;
    std::string skyError;size_t skyBytes=0;bool skyReady=false;
    std::string postError;
    size_t postBytes = 0;
    int postActive = 0;
    int annotationDepth = 0;
    uint64_t openingRequest = 0, openingPresented = 0;
    std::array<float,4> bounds{0,0,0,1};
    double gpuMs = -1, uploadMs = 0;
    size_t uploadedRows = 0, reusedRows = 0, decodedFiles = 0, subsetHits = 0;
    double requestRevision=0,displayRevision=0,prepareMs=0,refineMs=0,uploadedBytes=0,pageHits=0;
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
    void SetIntro(bool enabled, bool waitForModel);
    bool IsPresented(uint64_t request,uint64_t surface);
    void OnPresented(std::function<void(uint64_t,uint64_t)> callback);
    bool BeginIntro(uint64_t request, std::array<float,3> focus, const std::vector<float>& box={}, int profile=0);
    uint64_t BeginSkybox();
    bool SkyboxCurrent(uint64_t request);
    bool SetSkybox(uint64_t request,std::shared_ptr<const SkyImage> image);
    void SetEffects(Effects settings);
    void SetBackground(std::array<float,3> color);
    void SetChunks(std::vector<std::string> paths, std::array<float,4> bounds, std::vector<uint32_t> ranges = {},bool paged=false,uint64_t revision=0,bool encoded=false);
    void SetActive(bool active);
    void SetAnnotations(std::shared_ptr<const HotspotData> data);
    void SetAnnotationStyle(HotspotStyle style);
    void DropCaches();
    void TraceFrames(bool enabled);
    void SetOptimized(bool enabled);
    Status GetStatus();
    std::vector<float> Pick(float x,float y);
private:
    void ExtendOpening();
    void Loop(void *window);
    void SortLoop();
    void LoadLoop();
    void InitGL(void *window);
    void DestroyGL();
    struct PageLoad {
        std::shared_ptr<Scene> scene;
        std::vector<PageKey> keys;
        std::vector<std::shared_ptr<Scene>> pages;
        std::vector<uint32_t> indices;
        uint64_t generation=0,revision=0;
        double requestAt=0,prepareMs=0,sortMs=0;
        std::array<float,3> sortDirection{};
        size_t cursor=0,uploaded=0,hits=0;
        bool planned=false,encoded=false;
        std::vector<PageKey> books;
        std::vector<uint32_t> bookSlots;
        std::vector<uint32_t> uploadOrder;
    };
    void PreparePages(const std::vector<std::string>& paths,const std::array<float,4>& bounds,const std::vector<uint32_t>& ranges,uint64_t generation,uint64_t revision,double requestAt,bool encoded);
    void AdvancePages();
    std::shared_ptr<PageLoad> preparedPage_,stagingPage_;
    PageAtlas atlas_,encodedAtlas_,bookAtlas_;
    GLuint encodedCenters_=0,encodedCodes_=0,codebookTexture_=0;
    std::vector<PageKey> activeEncodedPages_,activeBooks_;
    std::vector<uint32_t> encodedBookSlots_;
    bool encodedDrawable_=false;
    bool chunksEncoded_=false;
    uint32_t encodedRows_=0;
    GLuint atlasTexture_=0;
    bool atlasDrawable_=false;
    uint32_t atlasRows_=0;
    std::vector<PageKey> activePages_;
    SceneCache<PageKey> pageCache_{384*1024*1024};
    bool chunksPaged_=false;
    bool dropCaches_=false,traceFrames_=false;
    double lastTraceFrame_=0;
    uint64_t requestRevision_=0;
    double requestAt_=0;
    uint64_t pendingDisplayRevision_=0;
    double pendingDisplayAt_=0,pendingPrepareMs_=0,pendingSortMs_=0;
    void AdvanceUpload();
    void Draw(const View &view, int width, int height);
    std::mutex mutex_;
    std::condition_variable changed_;
    std::thread worker_, sorter_, loader_;
    std::condition_variable loadChanged_;
    std::shared_ptr<Scene> preparedScene_;
    bool preparedResetCamera_ = false;
    std::vector<float> preparedPixels_, uploadPixels_;
    UploadRows preparedRows_, stagingRows_, dataRows_, spareRows_, stagingPreviousRows_;
    size_t stagingUploadedRows_ = 0, stagingReusedRows_ = 0;
    std::map<std::string,uint64_t> sourceIds_;
    uint64_t nextSourceId_ = 1;
    std::vector<uint32_t> preparedIndices_, initialIndices_;
    std::shared_ptr<Scene> stagingScene_;
    std::vector<float> stagingPixels_;
    std::vector<uint32_t> stagingIndices_;
    GLuint stagingTexture_ = 0, spareTexture_ = 0;
    size_t stagingCapacity_ = 0, spareCapacity_ = 0, dataCapacity_ = 0;
    size_t stagingRow_ = 0;
    uint64_t stagingGeneration_ = 0;
    bool stagingResetCamera_ = false, preuploaded_ = false;
    uint64_t loadGeneration_ = 0;
    std::atomic<size_t> cacheBytes_{0};
    std::vector<std::vector<float>> retiredPixels_;
    std::vector<std::shared_ptr<Scene>> retiredScenes_;
    std::condition_variable sortChanged_;
    std::shared_ptr<Scene> sortScene_, sortedScene_;
    View sortView_{};
    std::vector<uint32_t> sortedIndices_;
    bool sortPending_ = false, sortReady_ = false;
    double completedSortMs_ = 0;
    std::atomic<bool> cancel_{false};
    std::atomic<bool> optimized_{true};
    bool stop_ = false, dirty_ = true, active_ = true;
    int width_ = 1, height_ = 1;
    std::string pendingPath_;
    std::string requestedPath_;
    std::vector<std::string> chunkPaths_;
    std::array<float,4> chunkBounds_{};
    std::vector<uint32_t> chunkRanges_;
    SceneCache<std::string> decoded_{256*1024*1024};
    SceneCache<std::pair<std::string,std::vector<uint32_t>>> selected_{256*1024*1024};
    bool chunksDirty_ = false;
    Camera camera_;
    std::array<float,3> background_{0,0,0};
    std::function<void(uint64_t,uint64_t)> presentedCallback_;
    Intro intro_;
    Effects effects_{};
    bool effectsFailed_ = false;
    PostProcess post_;
    Skybox sky_;
    std::shared_ptr<const SkyImage> skyImage_;
    uint64_t skyRequest_=0;
    bool skyFailed_=false;
    bool openingCommitted_ = false;
    uint64_t surfaceGeneration_ = 0, presentedSurface_ = 0;
    uint64_t openingMinGeneration_ = 0;
    std::array<float,4> introBounds_{0,0,0,1};
    Status status_;
    std::shared_ptr<Scene> scene_ = std::make_shared<Scene>();
    bool uploadDirty_ = true;
    Hotspots hotspots_;
    std::shared_ptr<const HotspotData> annotationData_;
    HotspotStyle annotationStyle_;
    int depthBits_=0;
    std::array<float,3> sortDirection_{};
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
    using TimerResult = void (*)(GLuint, GLenum, GLuint64 *);
    TimerResult timerResult_ = nullptr;
    GLuint timerQueries_[4]{};
    bool timerPending_[4]{}, timerInvalid_[4]{};
    int timerSlot_ = 0;
    GLint viewLocation_=-1, viewportLocation_=-1, nearLocation_=-1, farLocation_=-1, dataLocation_=-1, optimizedLocation_=-1;
    GLuint program_ = 0, vao_ = 0, buffer_ = 0, dataTexture_ = 0;
    void *window_ = nullptr;
    int bufferWidth_ = 0, bufferHeight_ = 0;
};
}
