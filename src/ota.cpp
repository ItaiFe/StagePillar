#include "ota.h"
#include <ArduinoOTA.h>
#include "config.h"

static bool started = false;
static volatile bool active = false;

static void otaBegin() {
    ArduinoOTA.setHostname(DEVICE_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([]() {
        active = true;
        Serial.println("OTA: update started");
    });
    ArduinoOTA.onEnd([]() { Serial.println("\nOTA: done, rebooting"); });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
        Serial.printf("OTA: %u%%\r", done * 100 / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        active = false;
        Serial.printf("\nOTA: error %u\n", error);
    });
    ArduinoOTA.begin();
    started = true;
    Serial.printf("OTA: ready at %s.local\n", DEVICE_HOSTNAME);
}

void otaLoop(bool wifiConnected) {
    if (!started) {
        if (wifiConnected) otaBegin();
        return;
    }
    ArduinoOTA.handle();
}

bool otaActive() {
    return active;
}
