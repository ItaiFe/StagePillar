#include <Arduino.h>
#include "actions.h"
#include "config.h"
#include "gesture.h"
#include "net.h"
#include "ota.h"
#include "pillar_leds.h"
#include "sender.h"
#include "status_led.h"

static GestureDetector detector;

void setup() {
    Serial.begin(115200);
    Serial.printf("\nStagePillar button %s\n", FIRMWARE_VERSION);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    statusLedBegin();
    pillarLedsBegin();
    netBegin();
    senderBegin();
}

void loop() {
    uint32_t now = millis();
    bool wifi = netConnected();

    Gesture gesture = detector.update(digitalRead(BUTTON_PIN) == BUTTON_PRESSED_LEVEL, now);
    if (gesture != Gesture::None) {
        Serial.printf("Gesture -> %s\n", actionFor(gesture));
        pillarLedsPlay(gesture, now);
        if (!senderEnqueue(gesture)) Serial.println("Drop: queue full");
    }

    otaLoop(wifi);
    statusLedUpdate(now, wifi);
    pillarLedsUpdate(now, otaActive());
    delay(1);
}
