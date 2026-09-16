#pragma once
#include <algorithm>
#include <cmath>
namespace splat {
// Render-thread timeline guarded by the renderer mutex. No wall time accrues
// while the application or surface is inactive.
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
    void BeginVisible(float radius = 1) { if(held_ || progress_ == 0)duration_ = Duration(radius); held_ = false; last_ = -1; }
    bool Running() const { return progress_ < 1.0 && !held_; }
    float Frame(double seconds) {
        if (Running() && last_ >= 0) progress_ = std::min(1.0, progress_ + std::clamp(seconds-last_, 0.0, 1.0/30.0) / duration_);
        last_ = seconds;
        return static_cast<float>(progress_);
    }
    double Seconds() const { return progress_ * duration_; }
    static double Duration(float radius) { return std::max(5.0, 1.0 + (-.36 + std::sqrt(.36*.36 + 4.2*(std::max(1.f,radius)+2.0))) / 2.1); }
private:
    double duration_ = 5;
    bool pending_ = false, held_ = false;
    double progress_ = 1, last_ = -1;
};
}
