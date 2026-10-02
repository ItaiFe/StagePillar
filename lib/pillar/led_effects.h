#pragma once
#include <stdint.h>
#include "gesture.h"

// Pillar LED animations as pure functions over a plain RGB frame, so they can be
// unit-tested on the host. Index 0 is the bottom of the pillar.

struct Rgb {
    uint8_t r, g, b;
};

enum class Effect : uint8_t { None, Start, Claps, Special, Skip, Stop };

constexpr uint16_t kPillarLeds = 100;
// The idle rainbow moves up one LED every this many ms.
constexpr uint32_t kIdleMsPerLed = 80;

Effect effectFor(Gesture gesture);
uint32_t effectDurationMs(Effect effect);

// Slow rainbow flowing up the pillar.
void renderIdle(uint32_t nowMs, Rgb* frame, uint16_t count);
// Draws the effect at msSinceStart. Returns false once the effect is over, without
// touching the frame. seed varies the random sparkles between runs.
bool renderEffect(Effect effect, uint32_t msSinceStart, uint32_t seed, Rgb* frame, uint16_t count);
// Two quick red flashes over whatever is in the frame. Returns false once over.
bool overlayFail(uint32_t msSinceFail, Rgb* frame, uint16_t count);
