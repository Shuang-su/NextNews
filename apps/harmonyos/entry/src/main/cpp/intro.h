#pragma once
#include <algorithm>
namespace splat {
// Render-thread timeline guarded by the renderer mutex. No wall time accrues
// while the application or surface is inactive.
class Intro {
public:
    void Request(bool enabled, bool waitForModel) {
        pending_ = enabled && waitForModel;
        progress_ = enabled && !waitForModel ? 0.0 : 1.0;
        last_ = -1;
    }
    void Commit() { if (pending_) { pending_ = false; progress_ = 0; last_ = -1; } }
    void Pause() { last_ = -1; }
    bool Running() const { return progress_ < 1.0; }
    float Frame(double seconds) {
        if (Running() && last_ >= 0) progress_ = std::min(1.0, progress_ + std::max(0.0, seconds-last_) / 1.2);
        last_ = seconds;
        return static_cast<float>(progress_);
    }
private:
    bool pending_ = false;
    double progress_ = 1, last_ = -1;
};
}
