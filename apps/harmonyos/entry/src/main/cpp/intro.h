#pragma once
#include <algorithm>
#include <cmath>
#include <array>
namespace splat {
// Render-thread timeline guarded by the renderer mutex. No wall time accrues
// while the application or surface is inactive.
struct RevealMotion {
    float speed=.36f,acceleration=2.1f,delay=1.f,dotSize=.0012f,oscillation=.025f;
    static RevealMotion For(float radius,int profile) {
        RevealMotion m;
        radius=std::max(1.f,radius);
        m.oscillation=std::clamp(radius*.002f,.025f,.16f);
        m.dotSize=profile==1?.00063f:profile==2?std::clamp(radius*.00022f,.004f,.05f):std::clamp(radius*.000066f,.0012f,.015f);
        if(profile==2){
            m.speed=.6f;m.acceleration=3.5f;
            if(radius>20){const double t=(-.6+std::sqrt(.36+7*20))/3.5;m.acceleration=std::max(.0001,2*(radius-.6*t)/(t*t));}
            m.speed*=.85f;m.acceleration*=.85f;m.delay=1/.85f;
        }
        return m;
    }
    double Duration(float radius)const{return std::max(5.,double(delay)+(-speed+std::sqrt(double(speed)*speed+2*acceleration*std::max(1.f,radius)))/acceleration);}
};
inline float FarthestCorner(const std::array<float,3>& focus,const std::array<float,6>& box){
    double sum=0;
    for(int k=0;k<3;++k){const double d=std::max(std::abs(double(focus[k])-box[k]),std::abs(double(focus[k])-box[k+3]));sum+=d*d;}
    return std::max(1.f,float(std::sqrt(sum)));
}
class Intro {
public:
    void Request(bool enabled, bool waitForModel, float radius = 1) {
        duration_ = Duration(radius);
        pending_ = enabled && waitForModel;
        held_ = pending_;
        progress_ = enabled && !waitForModel ? 0.0 : 1.0;
        last_ = -1;
    }
    void Commit(float radius = 1) { if (pending_) { duration_ = Duration(radius); pending_ = false; progress_ = 0; last_ = -1; } }
    void Pause() { last_ = -1; }
    void BeginVisible(float radius = 1, int profile = 0) { if(held_ || progress_ == 0){profile_=profile;motion_=RevealMotion::For(radius,profile);duration_ = motion_.Duration(radius);} held_ = false; last_ = -1; }
    void Extend(float radius) {
        if(progress_>=1)return;
        const double seconds=Seconds();duration_=std::max(duration_,motion_.Duration(radius));progress_=seconds/duration_;
        const auto next=RevealMotion::For(radius,profile_);motion_.dotSize=next.dotSize;motion_.oscillation=next.oscillation;
    }
    bool Running() const { return progress_ < 1.0 && !held_; }
    float Frame(double seconds) {
        if (Running() && last_ >= 0) progress_ = std::min(1.0, progress_ + std::clamp(seconds-last_, 0.0, 1.0/30.0) / duration_);
        last_ = seconds;
        return static_cast<float>(progress_);
    }
    double Seconds() const { return progress_ * duration_; }
    RevealMotion Motion()const{return motion_;}
    static double Duration(float radius) { return RevealMotion::For(radius,0).Duration(radius); }
private:
    RevealMotion motion_;
    int profile_=0;
    double duration_ = 5;
    bool pending_ = false, held_ = false;
    double progress_ = 1, last_ = -1;
};
}
