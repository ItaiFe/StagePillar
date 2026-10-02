#include "status_led.h"
#include <Arduino.h>
#include "config.h"

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static Flash flash = Flash::None;
static uint32_t flashStart = 0;

void statusLedBegin() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
}

void statusLedFlash(Flash f) {
    portENTER_CRITICAL(&mux);
    flash = f;
    flashStart = millis();
    portEXIT_CRITICAL(&mux);
}

void statusLedUpdate(uint32_t nowMs, bool wifiConnected) {
    portENTER_CRITICAL(&mux);
    Flash f = flash;
    uint32_t start = flashStart;
    portEXIT_CRITICAL(&mux);
    digitalWrite(LED_PIN, ledLevel(f, nowMs - start, wifiConnected, nowMs) ? HIGH : LOW);
}
