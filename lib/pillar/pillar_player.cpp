#include "pillar_player.h"

// Idle lasts exactly 256 LEDs of rainbow shift so the pattern is seamless when it loops.
static const EffectStep kIdleSteps[] = {
    {EffectId::Rainbow, 256 * 80, 255, Direction::Up, 16, {}},
};
static const EffectStep kStartSteps[] = {
    {EffectId::Comet, 2100, 255, Direction::Bounce, 24, {{255, 0, 192}, {0, 229, 255}, {255, 176, 0}}},
};
static const EffectStep kClapsSteps[] = {
    {EffectId::Sparkle, 1200, 255, Direction::Up, 16, {{255, 255, 255}}},
};
static const EffectStep kSpecialSteps[] = {
    {EffectId::Pulse, 1500, 255, Direction::Up, 16, {{0, 0, 255}, {128, 0, 255}, {255, 0, 192}}},
};
static const EffectStep kSkipSteps[] = {
    {EffectId::Band, 500, 255, Direction::Up, 16, {{0, 255, 255}}},
};
static const EffectStep kStopSteps[] = {
    {EffectId::Fade, 1500, 255, Direction::Up, 16, {{255, 0, 0}, {40, 0, 0}}},
};

static const Sequence kIdle{true, 1, kIdleSteps};
static const Sequence kStart{true, 1, kStartSteps};
static const Sequence kClaps{false, 1, kClapsSteps};
static const Sequence kSpecial{false, 1, kSpecialSteps};
static const Sequence kSkip{false, 1, kSkipSteps};
static const Sequence kStop{false, 1, kStopSteps};

Slot slotFor(Gesture gesture) {
    switch (gesture) {
        case Gesture::Single: return Slot::Start;
        case Gesture::Double: return Slot::Claps;
        case Gesture::Triple:
        case Gesture::Many:   return Slot::Special;
        case Gesture::Quad:   return Slot::Skip;
        case Gesture::Long:   return Slot::Stop;
        default:              return Slot::None;
    }
}

const Sequence& defaultSequence(Slot slot) {
    switch (slot) {
        case Slot::Start:   return kStart;
        case Slot::Claps:   return kClaps;
        case Slot::Special: return kSpecial;
        case Slot::Skip:    return kSkip;
        case Slot::Stop:    return kStop;
        default:            return kIdle;
    }
}

bool renderSequence(const Sequence& seq, uint32_t msSinceStart, uint32_t seed, Rgb* frame, uint16_t count) {
    uint32_t total = 0;
    for (uint8_t i = 0; i < seq.stepCount; i++) total += seq.steps[i].durationMs;
    if (total == 0) return false;

    uint32_t t = msSinceStart;
    if (seq.loop) {
        t %= total;
    } else if (t >= total) {
        return false;
    }

    for (uint8_t i = 0; i < seq.stepCount; i++) {
        const EffectStep& step = seq.steps[i];
        if (t < step.durationMs) {
            renderStep(step, t, seed, frame, count);
            return true;
        }
        t -= step.durationMs;
    }
    return false;
}

PillarPlayer::PillarPlayer(Lookup lookup) : lookup_(lookup) {}

void PillarPlayer::play(Slot slot, uint32_t nowMs) {
    active_ = slot;
    activeStartMs_ = nowMs;
}

void PillarPlayer::trigger(Slot slot, uint32_t nowMs) {
    if (slot == Slot::None) return;
    if (slot == Slot::Start) running_ = true;
    if (slot == Slot::Stop) running_ = false;
    play(slot, nowMs);
}

void PillarPlayer::setShowRunning(bool running, uint32_t nowMs) {
    if (running == running_) return;
    running_ = running;
    if (running) {
        // A gesture in progress finishes first and then falls back to the play loop.
        if (active_ == Slot::Idle) play(Slot::Start, nowMs);
    } else if (active_ != Slot::Stop) {
        play(Slot::Stop, nowMs);
    }
}

void PillarPlayer::render(uint32_t nowMs, Rgb* frame, uint16_t count) {
    if (renderSequence(lookup_(active_), nowMs - activeStartMs_, activeStartMs_, frame, count)) return;
    play(running_ ? Slot::Start : Slot::Idle, nowMs);
    renderSequence(lookup_(active_), 0, activeStartMs_, frame, count);
}
