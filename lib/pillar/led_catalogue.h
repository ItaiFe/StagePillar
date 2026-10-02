#pragma once
#include <stdint.h>
#include "rgb.h"

// The pillar's built-in effects. Ids and fields mirror the EFFECT step of the
// StageController pillar LED contract, so uploaded plans and code defaults share
// one renderer. Index 0 is the bottom of the pillar.

enum class EffectId : uint8_t { Off = 0, Solid, Rainbow, Comet, Fill, Sparkle, Pulse, Band, Fade };
enum class Direction : uint8_t { Up = 0, Down = 1, Bounce = 2 };

struct EffectStep {
    EffectId effect;
    uint16_t durationMs;
    uint8_t brightness;   // 0..255, applied on top of the strip's global cap
    Direction direction;
    uint8_t speedX16;     // speed * 16; 16 = reference speed
    Rgb colors[3];        // unused entries are black
};

// Draws the step at msSinceStart (0 .. durationMs). seed varies random effects.
void renderStep(const EffectStep& step, uint32_t msSinceStart, uint32_t seed, Rgb* frame, uint16_t count);

// Two quick red flashes over whatever is in the frame. Returns false once over.
bool overlayFail(uint32_t msSinceFail, Rgb* frame, uint16_t count);
