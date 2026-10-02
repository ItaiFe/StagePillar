#include "pillar_leds.h"
#include <FastLED.h>
#include "config.h"
#include "pillar_player.h"
#include "plan_store.h"

static const uint16_t kLeds = 100;
static const uint32_t kFrameMs = 20;

static CRGB leds[kLeds];
static Rgb frame[kLeds];  // what the plans drew; frame steps snapshot this
static Rgb out[kLeds];    // frame plus the fail overlay
static uint32_t lastFrameMs = 0;

// Touched only from loop().
static PillarPlayer player(planStoreLookup);
static bool failActive = false;
static uint32_t failStartMs = 0;

// Set from the sender task, consumed by loop().
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static bool failPending = false;

void pillarLedsBegin() {
    FastLED.addLeds<WS2812B, PILLAR_LED_PIN, GRB>(leds, kLeds);
    FastLED.setBrightness(PILLAR_BRIGHTNESS);
    FastLED.clear(true);
    player.reload(millis());  // plans were loaded from flash after the player was built
}

void pillarLedsPlay(Gesture gesture, uint32_t nowMs) {
    player.trigger(slotFor(gesture), nowMs);
}

void pillarLedsFail() {
    portENTER_CRITICAL(&mux);
    failPending = true;
    portEXIT_CRITICAL(&mux);
}

void pillarLedsUpdate(uint32_t nowMs, bool otaActive, bool showRunning) {
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

    if (planStoreApplyPending()) player.reload(nowMs);
    Plan preview;
    if (planStoreTakePreview(preview)) player.playPreview(preview, nowMs);
    if (planStoreTakePreviewEnd()) player.endPreview(nowMs);

    player.setShowRunning(showRunning, nowMs);
    player.render(nowMs, frame, kLeds);
    for (uint16_t i = 0; i < kLeds; i++) out[i] = frame[i];
    if (failActive && !overlayFail(nowMs - failStartMs, out, kLeds)) failActive = false;

    for (uint16_t i = 0; i < kLeds; i++) leds[i] = CRGB(out[i].r, out[i].g, out[i].b);
    FastLED.show();
}
