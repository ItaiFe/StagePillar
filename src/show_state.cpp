#include "show_state.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include "config.h"
#include "net.h"
#include "plan_store.h"
#include "show_gate.h"

static const uint32_t kAssumeMs = 3000;

static volatile bool playing = false;
static volatile bool assumed = false;
static volatile uint32_t assumedAtMs = 0;

// Polls the player state, reporting which plans version the pillar runs, and syncs
// plans and previews from what it says. Returns whether the show is running.
static bool pollState() {
    if (!netConnected()) return false;
    IPAddress server = netServerIp();
    String url = "http://" + server.toString() + ":" + String(SERVER_PORT) +
                 "/api/player/state?client=pillar&running_version=" + String(planStoreVersion());
    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(url)) return false;
    int code = http.GET();
    String body = code == 200 ? http.getString() : String();
    http.end();
    if (code != 200) return false;

    uint32_t version = 0, previewId = 0;
    if (parseUintField(body.c_str(), "pillar_plans_version", version)) {
        parseUintField(body.c_str(), "pillar_preview_id", previewId);
        planStoreSync(server, version, previewId);
    }
    return parseShowRunning(body.c_str());
}

static void pollTask(void*) {
    for (;;) {
        bool now = pollState();
        if (now != playing) Serial.printf("Show: %s\n", now ? "running" : "idle");
        playing = now;
        vTaskDelay(pdMS_TO_TICKS(SHOW_POLL_MS));
    }
}

void showStateBegin() {
    xTaskCreate(pollTask, "show", 8192, nullptr, 1, nullptr);
}

bool showPlaying(uint32_t nowMs) {
    return playing || (assumed && nowMs - assumedAtMs < kAssumeMs);
}

void showAssumePlaying(uint32_t nowMs) {
    assumedAtMs = nowMs;
    assumed = true;
}
