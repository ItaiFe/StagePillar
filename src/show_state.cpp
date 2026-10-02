#include "show_state.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include "config.h"
#include "net.h"
#include "show_gate.h"

static const uint32_t kAssumeMs = 3000;

static volatile bool playing = false;
static volatile bool assumed = false;
static volatile uint32_t assumedAtMs = 0;

static bool fetchIsPlaying() {
    if (!netConnected()) return false;
    String url = "http://" + netServerIp().toString() + ":" + String(SERVER_PORT) + "/api/player/state";
    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(url)) return false;
    bool result = false;
    int code = http.GET();
    if (code == 200) result = parseIsPlaying(http.getString().c_str());
    http.end();
    return result;
}

static void pollTask(void*) {
    for (;;) {
        bool now = fetchIsPlaying();
        if (now != playing) Serial.printf("Show: %s\n", now ? "playing" : "idle");
        playing = now;
        vTaskDelay(pdMS_TO_TICKS(SHOW_POLL_MS));
    }
}

void showStateBegin() {
    xTaskCreate(pollTask, "show", 6144, nullptr, 1, nullptr);
}

bool showPlaying(uint32_t nowMs) {
    return playing || (assumed && nowMs - assumedAtMs < kAssumeMs);
}

void showAssumePlaying(uint32_t nowMs) {
    assumedAtMs = nowMs;
    assumed = true;
}
