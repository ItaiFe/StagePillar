#pragma once
#include <stdint.h>

// Whether the show is running, from server polls plus the button's own start/stop.
// After a local start or stop the poll lags by up to a couple of seconds, so for
// kAssumeMs the local action wins over what the poll reports.
class ShowLatch {
public:
    static const uint32_t kAssumeMs = 3000;

    void report(bool running) { reported_ = running; }
    void assumeStarted(uint32_t nowMs) { assume(true, nowMs); }
    void assumeStopped(uint32_t nowMs) { assume(false, nowMs); }

    bool running(uint32_t nowMs) const {
        if (assuming_ && nowMs - assumedAtMs_ < kAssumeMs) return assumed_;
        return reported_;
    }

private:
    void assume(bool running, uint32_t nowMs) {
        assuming_ = true;
        assumed_ = running;
        assumedAtMs_ = nowMs;
    }

    volatile bool reported_ = false;
    bool assuming_ = false;
    bool assumed_ = false;
    uint32_t assumedAtMs_ = 0;
};
