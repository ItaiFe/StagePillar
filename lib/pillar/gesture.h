#pragma once
#include <stdint.h>

// Many = 5 or more presses.
enum class Gesture : uint8_t { None, Single, Double, Triple, Quad, Many, Long };

struct GestureTiming {
    uint32_t debounceMs = 30;
    uint32_t gapMs = 400;
    uint32_t longMs = 1500;
};

// Turns a raw button level, sampled every loop, into press gestures.
// Pure logic with no Arduino dependencies so it can be unit-tested on the host.
class GestureDetector {
public:
    explicit GestureDetector(GestureTiming timing = GestureTiming());

    // pressed: raw level, true while the button is down. nowMs: millis().
    // Returns the gesture on the call where it completes, else Gesture::None.
    Gesture update(bool pressed, uint32_t nowMs);

private:
    GestureTiming timing_;
    bool raw_ = false;          // last sampled level
    uint32_t rawSince_ = 0;     // when raw_ last changed
    bool stable_ = false;       // debounced level
    uint32_t stableSince_ = 0;  // edge time of the last debounced change
    uint8_t count_ = 0;         // presses in the current gesture
    bool longFired_ = false;    // Long already emitted for the current hold
};
