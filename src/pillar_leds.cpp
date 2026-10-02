#include "pillar_leds.h"
#include <FastLED.h>
#include "config.h"
#include "led_effects.h"

static const uint32_t kFrameMs = 20;

static CRGB leds[kPillarLeds];
static Rgb frame[kPillarLeds];
static uint32_t lastFrameMs = 0;

// Touched only from loop().
static Effect effect = Effect::None;
static uint32_t effectStartMs = 0;
static uint32_t effectSeed = 0;
static bool failActive = false;
static uint32_t failStartMs = 0;

// Set from the sender task, consumed by loop().
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static bool failPending = false;

void pillarLedsBegin() {
    FastLED.addLeds<WS2812B, PILLAR_LED_PIN, GRB>(leds, kPillarLeds);
    FastLED.setBrightness(PILLAR_BRIGHTNESS);
    FastLED.clear(true);
}

void pillarLedsPlay(Gesture gesture, uint32_t nowMs) {
    Effect next = effectFor(gesture);
    if (next == Effect::None) return;
    effect = next;
    effectStartMs = nowMs;
    effectSeed = esp_random();
}

void pillarLedsFail() {
    portENTER_CRITICAL(&mux);
    failPending = true;
    portEXIT_CRITICAL(&mux);
}

void pillarLedsUpdate(uint32_t nowMs, bool otaActive) {
    if (nowMs - lastFrameMs < kFrameMs) return;
    lastFrameMs = nowMs;

    if (otaActive) {
        FastLED.clear(true);
        return;
    }

    portENTER_CRITICAL(&mux);
    bool fail = failPending;
    failPending = false;
    portEXIT_CRITICAL(&mux);
    if (fail) {
        failActive = true;
        failStartMs = nowMs;
    }

    if (!renderEffect(effect, nowMs - effectStartMs, effectSeed, frame, kPillarLeds)) {
        effect = Effect::None;
        renderIdle(nowMs, frame, kPillarLeds);
    }
    if (failActive && !overlayFail(nowMs - failStartMs, frame, kPillarLeds)) failActive = false;

    for (uint16_t i = 0; i < kPillarLeds; i++) leds[i] = CRGB(frame[i].r, frame[i].g, frame[i].b);
    FastLED.show();
}
