#include "renderer.h"
#include <chrono>
#include <cstring>
#include <stdexcept>
namespace splat {
namespace {
using Clock=std::chrono::steady_clock;
double Now(){return std::chrono::duration<double,std::milli>(Clock::now().time_since_epoch()).count();}
}
void Renderer::PreparePages(const std::vector<std::string>& paths,const std::array<float,4>& bounds,const std::vector<uint32_t>& ranges,uint64_t generation,uint64_t revision,double requestAt){
    const double start=Now();auto work=std::make_shared<PageLoad>();work->scene=std::make_shared<Scene>();
    auto &scene=*work->scene;scene.paged=true;scene.center={bounds[0],bounds[1],bounds[2]};scene.radius=bounds[3];
    work->generation=generation;work->revision=revision;work->requestAt=requestAt;
    size_t requested=0;for(size_t i=2;i<ranges.size();i+=3)requested+=ranges[i];
    if(requested>MaxGaussians)throw std::runtime_error("Page selection exceeds draw budget");
    scene.positions.reserve(requested);scene.addresses.reserve(requested);
    std::map<PageKey,uint32_t> localPages;size_t decoded=0,hits=0;
    for(uint32_t file=0;file<paths.size();++file){
        auto &source=sourceIds_[paths[file]];if(!source)source=nextSourceId_++;
        std::shared_ptr<Scene> full;
        auto getFull=[&](){if(!full){full=decoded_.Get(paths[file]);if(!full){full=std::make_shared<Scene>(ReadModel(paths[file],&cancel_));decoded_.Put(paths[file],full);++decoded;}}return full;};
        std::vector<std::pair<uint32_t,uint32_t>> selected;
        if(ranges.empty())selected.push_back({0,uint32_t(getFull()->points.size())});
        else for(size_t r=0;r<ranges.size();r+=3)if(ranges[r]==file)selected.push_back({ranges[r+1],ranges[r+2]?ranges[r+2]:uint32_t(getFull()->points.size())});
        for(auto range:selected){
            uint32_t offset=range.first,left=range.second;
            if(scene.Count()+left>MaxGaussians)throw std::runtime_error("Page draw budget exceeded");
            while(left){
                if(cancel_)return;
                const PageKey key{source,offset/PagePoints};const uint32_t inPage=offset%PagePoints,n=std::min(left,PagePoints-inPage);
                auto found=localPages.find(key);uint32_t pageIndex;
                if(found!=localPages.end())pageIndex=found->second;
                else {
                    auto page=pageCache_.Get(key);
                    if(page)++hits;
                    else {
                        auto input=getFull();const size_t begin=size_t(key.page)*PagePoints;
                        if(begin>=input->points.size())throw std::runtime_error("Source page out of range");
                        const auto count=std::min(size_t(PagePoints),input->points.size()-begin);
                        page=std::make_shared<Scene>();page->points.assign(input->points.begin()+begin,input->points.begin()+begin+count);pageCache_.Put(key,page);
                    }
                    pageIndex=work->pages.size();work->keys.push_back(key);work->pages.push_back(page);localPages.emplace(key,pageIndex);
                }
                const auto &page=work->pages[pageIndex];
                if(inPage+n>page->points.size())throw std::runtime_error("Selection exceeds source page");
                scene.ranges.push_back({page,inPage,n,uint32_t(scene.Count())});
                for(uint32_t i=0;i<n;++i){const auto *p=page->points[inPage+i].position;scene.positions.push_back({p[0],p[1],p[2]});scene.addresses.push_back(pageIndex*PagePoints+inPage+i);}
                offset+=n;left-=n;
            }
        }
    }
    Camera camera;{std::lock_guard<std::mutex> lock(mutex_);camera=camera_;}
    const double sortStart=Now();const auto view=MakeView(scene,camera);work->sortDirection={view.matrix[2],view.matrix[6],view.matrix[10]};work->indices=SortIndices(scene,view);work->sortMs=Now()-sortStart;work->prepareMs=Now()-start;
    {std::lock_guard<std::mutex> lock(mutex_);if(cancel_||generation!=loadGeneration_)return;
        preparedPage_=work;status_.decodedFiles=decoded;status_.subsetHits=hits;status_.prepareMs=work->prepareMs;status_.loadMs=work->prepareMs;status_.message="Pages prepared";dirty_=true;}
    changed_.notify_one();
}
void Renderer::AdvancePages(){
    if(!stagingPage_)return;auto &work=*stagingPage_;
    {std::lock_guard<std::mutex> lock(mutex_);if(work.generation!=loadGeneration_){stagingPage_.reset();dirty_=true;return;}}
    const double start=Now();
    if(!work.planned){
        if(!atlas_.Plan(work.keys,activePages_)){
            std::lock_guard<std::mutex> lock(mutex_);status_.state="error";status_.message="GPU page budget exhausted; current scene retained";stagingPage_.reset();return;
        }
        if(!atlasTexture_){
            glGenTextures(1,&atlasTexture_);glBindTexture(GL_TEXTURE_2D,atlasTexture_);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,4096,atlasRows_);
            if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU atlas allocation failed");
        }
        std::vector<uint32_t> slots;slots.reserve(work.keys.size());
        for(const auto &key:work.keys)slots.push_back(atlas_.Slot(key)*PagePoints);
        for(auto &address:work.scene->addresses)address=slots[address/PagePoints]+address%PagePoints;
        work.planned=true;
    }
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,atlasTexture_);
    size_t uploaded=0;std::array<float,PagePoints*16> pixels{};
    while(work.cursor<work.keys.size()&&uploaded<256){
        const auto &key=work.keys[work.cursor];
        if(atlas_.Ready(key)){++work.hits;++work.cursor;continue;}
        pixels.fill(0);const auto &page=*work.pages[work.cursor];
        for(size_t i=0;i<page.points.size();++i){const auto &g=page.points[i];float *p=pixels.data()+i*16;std::copy_n(g.position,3,p);std::copy_n(g.color,4,p+3);std::copy_n(g.covariance,6,p+7);}
        const auto slot=atlas_.Slot(key);
        glTexSubImage2D(GL_TEXTURE_2D,0,(slot%4)*1024,slot/4,1024,1,GL_RGBA,GL_FLOAT,pixels.data());
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU page upload failed");
        atlas_.MarkReady(key);++work.cursor;++uploaded;++work.uploaded;
    }
    std::lock_guard<std::mutex> lock(mutex_);status_.uploadMs=Now()-start;dirty_=true;
    if(work.cursor==work.keys.size()&&work.generation==loadGeneration_){
        retiredScenes_.push_back(std::move(scene_));scene_=work.scene;initialIndices_=std::move(work.indices);activePages_=work.keys;atlasDrawable_=true;
        uploadDirty_=true;preuploaded_=true;sortPending_=sortReady_=false;sortScene_.reset();sortedScene_.reset();sortedIndices_.clear();
        const auto view=MakeView(*scene_,camera_);
        if(work.sortDirection!=std::array<float,3>{view.matrix[2],view.matrix[6],view.matrix[10]}){sortScene_=scene_;sortView_=view;sortPending_=true;sortChanged_.notify_one();}
        status_.count=scene_->Count();status_.state="ready";status_.message="Paged selection ready";pendingDisplayRevision_=work.revision;pendingDisplayAt_=work.requestAt;pendingPrepareMs_=work.prepareMs;pendingSortMs_=work.sortMs;
        status_.uploadedBytes=work.uploaded*PagePoints*64;status_.pageHits=work.hits;
        stagingPage_.reset();loadChanged_.notify_one();
    }
}
}
