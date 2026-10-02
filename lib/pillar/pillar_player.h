#pragma once
#include <stdint.h>
#include "gesture.h"
#include "led_catalogue.h"

// One sequence per show state or gesture, as in the StageController pillar LED contract.
enum class Slot : uint8_t { None, Idle, Start, Claps, Special, Skip, Stop };

struct Sequence {
    bool loop;
    uint8_t stepCount;
    const EffectStep* steps;
};

Slot slotFor(Gesture gesture);
// What the pillar plays when no plan has been uploaded for a slot.
const Sequence& defaultSequence(Slot slot);

// Draws the sequence at msSinceStart. A looping sequence wraps; a one-shot returns
// false once it is over, without touching the frame.
bool renderSequence(const Sequence& seq, uint32_t msSinceStart, uint32_t seed, Rgb* frame, uint16_t count);

// Decides what the pillar shows:
// - idle sequence while the show is not running, play (start) loop while it is;
// - a gesture plays its sequence, then returns to whichever of those applies;
// - the show ending (or a stop gesture) plays stop once, then idle.
class PillarPlayer {
public:
    typedef const Sequence& (*Lookup)(Slot);

    explicit PillarPlayer(Lookup lookup = defaultSequence);

    void trigger(Slot slot, uint32_t nowMs);
    void setShowRunning(bool running, uint32_t nowMs);
    void render(uint32_t nowMs, Rgb* frame, uint16_t count);

private:
    void play(Slot slot, uint32_t nowMs);

    Lookup lookup_;
    bool running_ = false;
    Slot active_ = Slot::Idle;
    uint32_t activeStartMs_ = 0;
};
