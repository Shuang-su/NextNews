#include "renderer.h"
#include <chrono>
#include <cstring>
#include <stdexcept>
#include "decode_queue.h"
#include <hilog/log.h>
#include <qos/qos.h>
namespace splat {
namespace {
using Clock=std::chrono::steady_clock;
double Now(){return std::chrono::duration<double,std::milli>(Clock::now().time_since_epoch()).count();}
}
void Renderer::PreparePages(const std::vector<std::string>& paths,const std::array<float,4>& bounds,const std::vector<uint32_t>& ranges,uint64_t generation,uint64_t revision,double requestAt,bool encoded){
    const double start=Now();auto work=std::make_shared<PageLoad>();work->scene=std::make_shared<Scene>();
    OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","PagePrepare revision=%{public}llu queueMs=%{public}.2f files=%{public}zu",(unsigned long long)revision,start-requestAt,paths.size());
    auto &scene=*work->scene;scene.paged=true;scene.center={bounds[0],bounds[1],bounds[2]};scene.radius=bounds[3];
    work->encoded=encoded;work->generation=generation;work->revision=revision;work->requestAt=requestAt;
    size_t requested=0;for(size_t i=2;i<ranges.size();i+=3)requested+=ranges[i];
    if(requested>(encoded?MaxDrawGaussians:MaxGaussians))throw std::runtime_error("Page selection exceeds draw budget");
    scene.positions.reserve(requested);scene.addresses.reserve(requested);
    std::map<PageKey,uint32_t> localPages;std::vector<uint32_t> newPages;size_t decoded=0,hits=0;
    // Only the loader owns cache mutation. At most two independent decoders
    // run ahead; their futures join before cancellation can reset the token.
    std::vector<uint64_t> sources(paths.size());std::vector<std::string> cacheKeys;
    std::vector<std::shared_ptr<Scene>> cachedInputs(paths.size());std::vector<uint32_t> decodeFiles;
    for(uint32_t file=0;file<paths.size();++file){
        cacheKeys.push_back(paths[file]+(encoded?"#encoded":""));
        auto &source=sourceIds_[cacheKeys.back()];if(!source)source=nextSourceId_++;sources[file]=source;
        bool needsFull=ranges.empty();
        for(size_t r=0;r<ranges.size()&&!needsFull;r+=3)if(ranges[r]==file){
            if(!ranges[r+2]){needsFull=true;break;}
            const uint64_t end=uint64_t(ranges[r+1])+ranges[r+2];
            for(uint64_t page=ranges[r+1]/PagePoints;page<(end+PagePoints-1)/PagePoints;++page)
                if(!pageCache_.Contains({source,uint32_t(page)})){needsFull=true;break;}
        }
        if(needsFull){cachedInputs[file]=decoded_.Get(cacheKeys.back());if(!cachedInputs[file])decodeFiles.push_back(file);}
    }
    DecodeQueue<std::shared_ptr<Scene>> jobs(cancel_,std::move(decodeFiles),[&](uint32_t file){
            OH_QoS_SetThreadQoS(QOS_USER_INITIATED);const double decodeAt=Now();
            if(cancel_)throw std::runtime_error("Load cancelled");
            auto result=std::make_shared<Scene>(ReadModel(paths[file],&cancel_,encoded,encoded));
            if(result->shDegree&&!encoded)throw std::runtime_error("High-order SH page streaming is not yet supported");
            OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","PageDecode revision=%{public}llu file=%{public}u count=%{public}zu decodeMs=%{public}.2f",(unsigned long long)revision,file,result->Count(),Now()-decodeAt);
            return result;
    });
    for(uint32_t file=0;file<paths.size();++file){
        if(cancel_)return;
        const auto &cacheKey=cacheKeys[file];const auto source=sources[file];
        std::shared_ptr<Scene> full;
        auto getFull=[&](){if(!full){full=std::move(cachedInputs[file]);if(!full){
            full=jobs.Take(file);++decoded;decoded_.Put(cacheKey,full);
        }}return full;};
        std::vector<std::pair<uint32_t,uint32_t>> selected;
        if(ranges.empty())selected.push_back({0,uint32_t(getFull()->Count())});
        else for(size_t r=0;r<ranges.size();r+=3)if(ranges[r]==file)selected.push_back({ranges[r+1],ranges[r+2]?ranges[r+2]:uint32_t(getFull()->Count())});
        for(auto range:selected){
            uint32_t offset=range.first,left=range.second;
            if(scene.Count()+left>(encoded?MaxDrawGaussians:MaxGaussians))throw std::runtime_error("Page draw budget exceeded");
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
                        if(begin>=input->Count())throw std::runtime_error("Source page out of range");
                        const auto count=std::min(size_t(PagePoints),input->Count()-begin);
                        page=std::make_shared<Scene>();
                        if(encoded){if(!input->tables)throw std::runtime_error("Encoded streaming requires SOG v2");page->tables=input->tables;
                            page->shDegree=input->shDegree;page->shTransform=input->shTransform;page->sogHarmonics=input->sogHarmonics;
                            if(page->shDegree)page->shLabels.assign(input->shLabels.begin()+begin,input->shLabels.begin()+begin+count);
                            page->positions.assign(input->positions.begin()+begin,input->positions.begin()+begin+count);page->codes.assign(input->codes.begin()+begin,input->codes.begin()+begin+count);}
                        else page->points.assign(input->points.begin()+begin,input->points.begin()+begin+count);
                        // Defer insertion: it must not evict a later source page
                        // whose cache hit made decoding unnecessary above.
                        newPages.push_back(uint32_t(work->pages.size()));
                    }
                    pageIndex=work->pages.size();work->keys.push_back(key);work->pages.push_back(page);localPages.emplace(key,pageIndex);
                }
                const auto &page=work->pages[pageIndex];
                scene.shDegree=std::max(scene.shDegree,page->shDegree);scene.shTransform=page->shTransform;
                if(inPage+n>page->Count())throw std::runtime_error("Selection exceeds source page");
                scene.ranges.push_back({page,inPage,n,uint32_t(scene.Count())});
                for(uint32_t i=0;i<n;++i){const auto *p=page->Position(inPage+i);scene.Include(p);scene.positions.push_back({p[0],p[1],p[2]});scene.addresses.push_back(pageIndex*PagePoints+inPage+i);}
                offset+=n;left-=n;
            }
        }
        // Duplicate source paths can become page hits while a read is in flight.
        if(jobs.Contains(file))jobs.Take(file);
        cachedInputs[file].reset();
    }
    for(const auto index:newPages)pageCache_.Put(work->keys[index],work->pages[index]);
    Camera camera;{std::lock_guard<std::mutex> lock(mutex_);camera=camera_;}
    const double sortStart=Now();const auto view=MakeView(scene,camera);work->sortDirection={view.matrix[2],view.matrix[6],view.matrix[10]};work->indices=SortIndices(scene,view);work->sortMs=Now()-sortStart;work->prepareMs=Now()-start;
    OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","PageSorted revision=%{public}llu prepareMs=%{public}.2f sortMs=%{public}.2f decoded=%{public}zu pageHits=%{public}zu",(unsigned long long)revision,work->prepareMs,work->sortMs,decoded,hits);
    {std::lock_guard<std::mutex> lock(mutex_);if(cancel_||generation!=loadGeneration_)return;
        preparedPage_=work;status_.decodedFiles=decoded;status_.subsetHits=hits;status_.prepareMs=work->prepareMs;status_.loadMs=work->prepareMs;status_.message="Pages prepared";dirty_=true;}
    changed_.notify_one();
}
void Renderer::AdvancePages(){
    if(!stagingPage_)return;auto &work=*stagingPage_;
    {std::lock_guard<std::mutex> lock(mutex_);if(work.generation!=loadGeneration_){stagingPage_.reset();dirty_=true;return;}}
    const double start=Now();auto &atlas=work.encoded?encodedAtlas_:atlas_;
    if(!work.planned){
        if(!atlas.Plan(work.keys,work.encoded?activeEncodedPages_:activePages_)){
            std::lock_guard<std::mutex> lock(mutex_);status_.state="error";status_.message="GPU page budget exhausted; current scene retained";stagingPage_.reset();return;
        }
        if(!work.encoded && !atlasTexture_){
            glGenTextures(1,&atlasTexture_);glBindTexture(GL_TEXTURE_2D,atlasTexture_);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,4096,atlasRows_);
            if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU atlas allocation failed");
        }
        if(work.encoded){
            auto texture=[](GLuint &id,GLenum format,int width,int height){if(id)return;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                glTexStorage2D(GL_TEXTURE_2D,1,format,width,height);if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Encoded atlas allocation failed");};
            texture(encodedCenters_,GL_RGBA32F,4096,encodedRows_);texture(encodedCodes_,GL_RGBA32UI,4096,encodedRows_);texture(codebookTexture_,GL_RGBA32F,256,2048);
            for(const auto &key:work.keys)work.books.push_back({key.source,0});
            std::sort(work.books.begin(),work.books.end());work.books.erase(std::unique(work.books.begin(),work.books.end()),work.books.end());
            if(!bookAtlas_.Plan(work.books,activeBooks_))throw std::runtime_error("SOG codebook capacity exceeded");
            for(const auto &key:work.keys)work.bookSlots.push_back(bookAtlas_.Slot({key.source,0}));
            for(size_t i=0;i<work.pages.size();i++)if(work.pages[i]->sogHarmonics)work.shSources.emplace(work.keys[i].source,work.pages[i]->sogHarmonics);
            for(const auto &source:work.shSources){
                const size_t count=(source.second->centroids.size()+ShPageTexels*4-1)/(ShPageTexels*4);
                if(count>ShSourcePages)throw std::runtime_error("SH source page limit");
                for(uint32_t page=0;page<count;page++)work.shKeys.push_back({source.first,page});
            }
            if(!shAtlas_.Plan(work.shKeys,activeShPages_)){
                std::lock_guard<std::mutex> lock(mutex_);status_.state="error";status_.message="SH resident cache full; current view retained";stagingPage_.reset();return;
            }
            if(!work.shKeys.empty()){
                texture(shCentroidAtlas_,GL_RGBA8UI,4096,4096);texture(shMapping_,GL_R32UI,ShSourcePages,2048);
                for(const auto &key:work.shKeys)work.shRows[bookAtlas_.Slot({key.source,0})][key.page]=shAtlas_.Slot(key);
            }
        }
        std::vector<uint32_t> slots;slots.reserve(work.keys.size());
        for(const auto &key:work.keys)slots.push_back(atlas.Slot(key)*PagePoints);
        for(auto &address:work.scene->addresses)address=slots[address/PagePoints]+address%PagePoints;
        work.uploadOrder.resize(work.keys.size());for(uint32_t i=0;i<work.keys.size();++i)work.uploadOrder[i]=i;
        std::sort(work.uploadOrder.begin(),work.uploadOrder.end(),[&](uint32_t a,uint32_t b){return slots[a]<slots[b];});
        work.planned=true;
    }
    glActiveTexture(GL_TEXTURE0);
    size_t uploaded=0;std::array<float,4096*4> pixels{};std::array<uint32_t,4096*4> codes{};
    const size_t pageBytes=PagePoints*(work.encoded?32:64),pagesPerRow=work.encoded?16:4,budget=4*1024*1024;
    while(work.shCursor<work.shKeys.size()){
        const auto key=work.shKeys[work.shCursor];
        if(shAtlas_.Ready(key)){++work.shCursor;continue;}
        if(uploaded+ShPageTexels*4>budget)break;
        std::array<uint8_t,ShPageTexels*4> bytes{};const auto &source=*work.shSources.at(key.source);
        const size_t begin=size_t(key.page)*bytes.size();std::copy_n(source.centroids.data()+begin,std::min(bytes.size(),source.centroids.size()-begin),bytes.data());
        glBindTexture(GL_TEXTURE_2D,shCentroidAtlas_);glTexSubImage2D(GL_TEXTURE_2D,0,0,shAtlas_.Slot(key)*4,4096,4,GL_RGBA_INTEGER,GL_UNSIGNED_BYTE,bytes.data());
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("SH centroid page upload failed");
        shAtlas_.MarkReady(key);++work.shCursor;uploaded+=bytes.size();work.uploaded+=bytes.size();
    }
    auto bookReady=[&](uint32_t index,size_t reserved){
        const PageKey key{work.keys[index].source,0};if(!work.encoded||bookAtlas_.Ready(key))return true;
        if(uploaded+reserved+4096>budget)return false;
        const auto &table=*work.pages[index]->tables;std::array<float,1024> values{};
        for(int i=0;i<256;++i){values[i*4]=std::exp(2*table.scale[i]);values[i*4+1]=std::clamp(.5f+.28209479177387814f*table.color[i],0.f,1.f);values[i*4+2]=work.pages[index]->sogHarmonics?work.pages[index]->sogHarmonics->books[i][1]:0;values[i*4+3]=.5f+.28209479177387814f*table.color[i];}
        glBindTexture(GL_TEXTURE_2D,codebookTexture_);glTexSubImage2D(GL_TEXTURE_2D,0,0,work.bookSlots[index],256,1,GL_RGBA,GL_FLOAT,values.data());
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("SOG codebook upload failed");
        bookAtlas_.MarkReady(key);uploaded+=4096;work.uploaded+=4096;return true;
    };
    auto mappingReady=[&](uint32_t index,size_t reserved){
        const auto slot=work.bookSlots[index];auto row=work.shRows.find(slot);if(row==work.shRows.end())return true;
        auto previous=shMappingRows_.find(slot);if(previous!=shMappingRows_.end()&&previous->second==row->second)return true;
        const size_t bytes=ShSourcePages*sizeof(uint32_t);if(uploaded+reserved+bytes>budget)return false;
        glBindTexture(GL_TEXTURE_2D,shMapping_);glTexSubImage2D(GL_TEXTURE_2D,0,0,slot,ShSourcePages,1,GL_RED_INTEGER,GL_UNSIGNED_INT,row->second.data());
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("SH page mapping upload failed");
        shMappingRows_[slot]=row->second;uploaded+=bytes;work.uploaded+=bytes;return true;
    };
    auto pageReady=[&](uint32_t index){return atlas.Ready(work.keys[index])&&(!work.encoded||encodedBookSlots_[atlas.Slot(work.keys[index])]==work.bookSlots[index]);};
    while(work.shCursor==work.shKeys.size()&&work.cursor<work.keys.size()){
        const uint32_t index=work.uploadOrder[work.cursor];
        if(!bookReady(index,0)||(work.encoded&&!mappingReady(index,0)))break;
        if(pageReady(index)){++work.hits;++work.cursor;continue;}
        const auto slot=atlas.Slot(work.keys[index]);const auto first=work.cursor;size_t run=0;pixels.fill(0);codes.fill(0);
        // Coalesce adjacent physical pages in a row. No resident gap is touched.
        while(work.cursor<work.keys.size()&&slot%pagesPerRow+run<pagesPerRow){
            const auto n=work.uploadOrder[work.cursor];const auto nextSlot=atlas.Slot(work.keys[n]);
            if(nextSlot!=slot+run||pageReady(n)||uploaded+(run+1)*pageBytes>budget||!bookReady(n,(run+1)*pageBytes)||(work.encoded&&!mappingReady(n,(run+1)*pageBytes)))break;
            const auto &page=*work.pages[n];
            for(size_t i=0;i<page.Count();++i){const auto point=run*PagePoints+i;
                if(work.encoded){std::copy_n(page.Position(i),3,pixels.data()+point*4);const auto &c=page.codes[i];
                    codes[point*4]=c.quaternion;codes[point*4+1]=c.scale;codes[point*4+2]=c.color;codes[point*4+3]=SogReference(work.bookSlots[n],page.shDegree?page.shLabels[i][0]:0,page.shDegree);}
                else{const auto &g=page.points[i];float *p=pixels.data()+point*16;std::copy_n(g.position,3,p);std::copy_n(g.color,4,p+3);std::copy_n(g.covariance,6,p+7);}
            }
            ++run;++work.cursor;
        }
        if(!run)break;
        const int width=int(run*PagePoints*(work.encoded?1:4)),x=int((slot%pagesPerRow)*PagePoints*(work.encoded?1:4)),y=int(slot/pagesPerRow);
        glBindTexture(GL_TEXTURE_2D,work.encoded?encodedCenters_:atlasTexture_);glTexSubImage2D(GL_TEXTURE_2D,0,x,y,width,1,GL_RGBA,GL_FLOAT,pixels.data());
        if(work.encoded){glBindTexture(GL_TEXTURE_2D,encodedCodes_);glTexSubImage2D(GL_TEXTURE_2D,0,x,y,width,1,GL_RGBA_INTEGER,GL_UNSIGNED_INT,codes.data());}
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU page upload failed");
        for(size_t i=first;i<work.cursor;++i){const auto n=work.uploadOrder[i];
            if(work.encoded)encodedBookSlots_[atlas.Slot(work.keys[n])]=work.bookSlots[n];atlas.MarkReady(work.keys[n]);}
        uploaded+=run*pageBytes;work.uploaded+=run*pageBytes;
    }
    std::lock_guard<std::mutex> lock(mutex_);status_.uploadMs=Now()-start;dirty_=true;
    if(work.shCursor==work.shKeys.size()&&work.cursor==work.keys.size()&&work.generation==loadGeneration_){
        activeShPages_=work.shKeys;
        OH_LOG_Print(LOG_APP,LOG_INFO,0xD003,"NextNewsPages","PageGpuReady revision=%{public}llu uploadedBytes=%{public}zu elapsedMs=%{public}.2f",(unsigned long long)work.revision,work.uploaded,Now()-work.requestAt);
        retiredScenes_.push_back(std::move(scene_));scene_=work.scene;scene_->FitClipping();ExtendOpening();if(scene_->Count() && work.generation>=openingMinGeneration_){intro_.Commit(scene_->radius);openingCommitted_=status_.openingPresented<status_.openingRequest;}initialIndices_=std::move(work.indices);if(work.encoded){activeEncodedPages_=work.keys;activeBooks_=work.books;activePages_.clear();}else{activePages_=work.keys;activeEncodedPages_.clear();activeBooks_.clear();}
        encodedDrawable_=work.encoded;atlasDrawable_=true;
        uploadDirty_=true;preuploaded_=true;sortPending_=sortReady_=false;sortScene_.reset();sortedScene_.reset();sortedIndices_.clear();
        const auto view=MakeView(*scene_,camera_);
        if(work.sortDirection!=std::array<float,3>{view.matrix[2],view.matrix[6],view.matrix[10]}){sortScene_=scene_;sortView_=view;sortPending_=true;sortChanged_.notify_one();}
        status_.count=scene_->Count();status_.state="ready";status_.message="Paged selection ready";pendingDisplayRevision_=work.revision;pendingDisplayAt_=work.requestAt;pendingPrepareMs_=work.prepareMs;pendingSortMs_=work.sortMs;
        status_.uploadedBytes=work.uploaded;status_.pageHits=work.hits;
        stagingPage_.reset();loadChanged_.notify_one();
    }
}
}
