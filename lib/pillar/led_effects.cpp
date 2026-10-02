#include "led_effects.h"

static const Rgb kBlack{0, 0, 0};
static const Rgb kWarmWhite{255, 160, 60};
static const Rgb kWhite{255, 255, 255};
static const Rgb kCyan{0, 255, 255};
static const Rgb kRed{255, 0, 0};

static Rgb scale(Rgb c, uint32_t level) {
    return Rgb{uint8_t(c.r * level / 255), uint8_t(c.g * level / 255), uint8_t(c.b * level / 255)};
}

// Fully saturated colour wheel: 0 red, 85 green, 170 blue.
static Rgb hueToRgb(uint8_t hue) {
    uint8_t region = hue / 43;
    uint8_t rise = (hue - region * 43) * 6;
    uint8_t fall = 255 - rise;
    switch (region) {
        case 0: return Rgb{255, rise, 0};
        case 1: return Rgb{fall, 255, 0};
        case 2: return Rgb{0, 255, rise};
        case 3: return Rgb{0, fall, 255};
        case 4: return Rgb{rise, 0, 255};
        default: return Rgb{255, 0, fall};
    }
}

static uint32_t hash(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t x = a * 2654435761u ^ b * 40503u ^ c * 2246822519u;
    x ^= x >> 15;
    x *= 2654435761u;
    x ^= x >> 13;
    return x;
}

static void fill(Rgb* frame, uint16_t count, Rgb c) {
    for (uint16_t i = 0; i < count; i++) frame[i] = c;
}

Effect effectFor(Gesture gesture) {
    switch (gesture) {
        case Gesture::Single: return Effect::Start;
        case Gesture::Double: return Effect::Claps;
        case Gesture::Triple:
        case Gesture::Many:   return Effect::Special;
        case Gesture::Quad:   return Effect::Skip;
        case Gesture::Long:   return Effect::Stop;
        default:              return Effect::None;
    }
}

uint32_t effectDurationMs(Effect effect) {
    switch (effect) {
        case Effect::Start:   return 1500;
        case Effect::Claps:   return 1200;
        case Effect::Special: return 1500;
        case Effect::Skip:    return 600;
        case Effect::Stop:    return 1500;
        default:              return 0;
    }
}

void renderIdle(uint32_t nowMs, Rgb* frame, uint16_t count) {
    uint32_t shift = nowMs / kIdleMsPerLed;
    for (uint16_t i = 0; i < count; i++) {
        frame[i] = hueToRgb(uint8_t((i - shift) * 5));
    }
}

// Warm white fills upward to the top in 900 ms, then fades out.
static void renderStart(uint32_t t, Rgb* frame, uint16_t count) {
    uint32_t head = t < 900 ? t * count / 900 : count;
    uint32_t level = t < 900 ? 255 : 255 * (1500 - t) / 600;
    for (uint16_t i = 0; i < count; i++) frame[i] = i < head ? scale(kWarmWhite, level) : kBlack;
}

// About 15% of LEDs sparkle white, re-rolled every 80 ms, fading over the effect.
static void renderClaps(uint32_t t, uint32_t seed, Rgb* frame, uint16_t count) {
    uint32_t level = 255 * (1200 - t) / 1200;
    uint32_t window = t / 80;
    for (uint16_t i = 0; i < count; i++) {
        frame[i] = hash(i, seed, window) % 100 < 15 ? scale(kWhite, level) : kBlack;
    }
}

// Whole pillar pulses twice while the hue drifts blue → purple → magenta.
static void renderSpecial(uint32_t t, Rgb* frame, uint16_t count) {
    uint32_t phase = t % 750;
    uint32_t level = phase < 375 ? phase * 255 / 375 : (750 - phase) * 255 / 375;
    fill(frame, count, scale(hueToRgb(uint8_t(160 + t * 80 / 1500)), level));
}

// A 15-LED cyan band runs bottom to top in 500 ms.
static void renderSkip(uint32_t t, Rgb* frame, uint16_t count) {
    const uint32_t width = 15;
    uint32_t head = t * (count + width) / 500;
    for (uint16_t i = 0; i < count; i++) {
        frame[i] = (i <= head && i + width > head) ? kCyan : kBlack;
    }
}

// Red fades from full to a dim glow over 1 s and holds there.
static void renderStop(uint32_t t, Rgb* frame, uint16_t count) {
    uint8_t r = t < 1000 ? uint8_t(255 - 215 * t / 1000) : 40;
    fill(frame, count, Rgb{r, 0, 0});
}

bool renderEffect(Effect effect, uint32_t msSinceStart, uint32_t seed, Rgb* frame, uint16_t count) {
    if (msSinceStart >= effectDurationMs(effect)) return false;
    switch (effect) {
        case Effect::Start:   renderStart(msSinceStart, frame, count); break;
        case Effect::Claps:   renderClaps(msSinceStart, seed, frame, count); break;
        case Effect::Special: renderSpecial(msSinceStart, frame, count); break;
        case Effect::Skip:    renderSkip(msSinceStart, frame, count); break;
        case Effect::Stop:    renderStop(msSinceStart, frame, count); break;
        default:              return false;
    }
    return true;
}

bool overlayFail(uint32_t msSinceFail, Rgb* frame, uint16_t count) {
    if (msSinceFail >= 300) return false;
    if ((msSinceFail / 100) % 2 == 0) fill(frame, count, kRed);
    return true;
}
