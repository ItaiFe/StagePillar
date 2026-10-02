#include "led_catalogue.h"

static const Rgb kBlack{0, 0, 0};
static const Rgb kWhite{255, 255, 255};

// Reference timings at speed 1.0.
static const uint32_t kRainbowMsPerLed = 80;
static const uint32_t kCometPassMs = 1000;
static const uint32_t kCometTail = 20;
static const uint32_t kSparkleWindowMs = 80;
static const uint32_t kPulseMs = 750;
static const uint32_t kBandPassMs = 500;
static const uint32_t kBandWidth = 15;

static Rgb scale(Rgb c, uint32_t level) {
    return Rgb{uint8_t(c.r * level / 255), uint8_t(c.g * level / 255), uint8_t(c.b * level / 255)};
}

static uint8_t lerp8(uint8_t a, uint8_t b, uint32_t t, uint32_t total) {
    return uint8_t(int32_t(a) + (int32_t(b) - int32_t(a)) * int32_t(t) / int32_t(total));
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

static bool isBlack(const Rgb& c) {
    return c.r == 0 && c.g == 0 && c.b == 0;
}

// Leading non-black colours, at least 1.
static uint8_t colorCount(const EffectStep& s) {
    uint8_t n = 0;
    while (n < 3 && !isBlack(s.colors[n])) n++;
    return n ? n : 1;
}

// Reference duration scaled by speed (faster speed = shorter period).
static uint32_t scaled(uint32_t referenceMs, uint8_t speedX16) {
    uint32_t ms = referenceMs * 16 / (speedX16 ? speedX16 : 16);
    return ms ? ms : 1;
}

// Whether pass number `pass` runs upward.
static bool passUp(Direction d, uint32_t pass) {
    if (d == Direction::Down) return false;
    if (d == Direction::Bounce) return pass % 2 == 0;
    return true;
}

static void fill(Rgb* frame, uint16_t count, Rgb c) {
    for (uint16_t i = 0; i < count; i++) frame[i] = c;
}

static void renderRainbow(const EffectStep& s, uint32_t t, Rgb* frame, uint16_t count) {
    uint32_t shift = t / scaled(kRainbowMsPerLed, s.speedX16);
    bool up = s.direction != Direction::Down;
    for (uint16_t i = 0; i < count; i++) {
        frame[i] = hueToRgb(uint8_t((up ? i - shift : i + shift) * 5));
    }
}

// A head with a fading tail runs end to end once per pass; colours change per pass.
static void renderComet(const EffectStep& s, uint32_t t, Rgb* frame, uint16_t count) {
    uint32_t passMs = scaled(kCometPassMs, s.speedX16);
    uint32_t pass = t / passMs;
    uint32_t head = (t % passMs) * (count + kCometTail) / passMs;
    bool up = passUp(s.direction, pass);
    Rgb color = s.colors[pass % colorCount(s)];
    for (uint16_t i = 0; i < count; i++) {
        uint32_t pos = up ? i : count - 1 - i;
        if (pos <= head && head - pos < kCometTail) {
            frame[i] = scale(color, 255 * (kCometTail - (head - pos)) / kCometTail);
        } else {
            frame[i] = kBlack;
        }
    }
}

// Fills end to end in the first 60% of the step, then fades out.
static void renderFill(const EffectStep& s, uint32_t t, Rgb* frame, uint16_t count) {
    uint32_t fillMs = s.durationMs * 3 / 5;
    uint32_t head = t < fillMs ? t * count / fillMs : count;
    uint32_t level = t < fillMs ? 255 : 255 * (s.durationMs - t) / (s.durationMs - fillMs);
    bool up = s.direction != Direction::Down;
    for (uint16_t i = 0; i < count; i++) {
        uint32_t pos = up ? i : count - 1 - i;
        frame[i] = pos < head ? scale(s.colors[0], level) : kBlack;
    }
}

// About 15% of LEDs flash, re-rolled every window, fading over the step.
static void renderSparkle(const EffectStep& s, uint32_t t, uint32_t seed, Rgb* frame, uint16_t count) {
    Rgb color = isBlack(s.colors[0]) ? kWhite : s.colors[0];
    uint32_t level = 255 * (s.durationMs - t) / s.durationMs;
    uint32_t window = t / scaled(kSparkleWindowMs, s.speedX16);
    for (uint16_t i = 0; i < count; i++) {
        frame[i] = hash(i, seed, window) % 100 < 15 ? scale(color, level) : kBlack;
    }
}

// The whole strip breathes once per period, in each colour in turn.
static void renderPulse(const EffectStep& s, uint32_t t, Rgb* frame, uint16_t count) {
    uint32_t period = scaled(kPulseMs, s.speedX16);
    uint32_t phase = t % period;
    uint32_t half = period / 2;
    uint32_t level = phase < half ? phase * 255 / half : (period - phase) * 255 / half;
    fill(frame, count, scale(s.colors[(t / period) % colorCount(s)], level));
}

// A short band runs end to end once per pass.
static void renderBand(const EffectStep& s, uint32_t t, Rgb* frame, uint16_t count) {
    uint32_t passMs = scaled(kBandPassMs, s.speedX16);
    uint32_t pass = t / passMs;
    uint32_t head = (t % passMs) * (count + kBandWidth) / passMs;
    bool up = passUp(s.direction, pass);
    for (uint16_t i = 0; i < count; i++) {
        uint32_t pos = up ? i : count - 1 - i;
        frame[i] = (pos <= head && pos + kBandWidth > head) ? s.colors[0] : kBlack;
    }
}

static void renderFade(const EffectStep& s, uint32_t t, Rgb* frame, uint16_t count) {
    Rgb a = s.colors[0], b = s.colors[1];
    fill(frame, count, Rgb{lerp8(a.r, b.r, t, s.durationMs), lerp8(a.g, b.g, t, s.durationMs),
                           lerp8(a.b, b.b, t, s.durationMs)});
}

void renderStep(const EffectStep& step, uint32_t msSinceStart, uint32_t seed, Rgb* frame, uint16_t count) {
    uint32_t t = msSinceStart < step.durationMs ? msSinceStart : (step.durationMs ? step.durationMs - 1 : 0);
    switch (step.effect) {
        case EffectId::Solid:   fill(frame, count, step.colors[0]); break;
        case EffectId::Rainbow: renderRainbow(step, t, frame, count); break;
        case EffectId::Comet:   renderComet(step, t, frame, count); break;
        case EffectId::Fill:    renderFill(step, t, frame, count); break;
        case EffectId::Sparkle: renderSparkle(step, t, seed, frame, count); break;
        case EffectId::Pulse:   renderPulse(step, t, frame, count); break;
        case EffectId::Band:    renderBand(step, t, frame, count); break;
        case EffectId::Fade:    renderFade(step, t, frame, count); break;
        default:                fill(frame, count, kBlack); break;
    }
    if (step.brightness < 255) {
        for (uint16_t i = 0; i < count; i++) frame[i] = scale(frame[i], step.brightness);
    }
}

bool overlayFail(uint32_t msSinceFail, Rgb* frame, uint16_t count) {
    if (msSinceFail >= 300) return false;
    if ((msSinceFail / 100) % 2 == 0) fill(frame, count, Rgb{255, 0, 0});
    return true;
}
