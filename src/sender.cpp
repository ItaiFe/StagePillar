#include "sender.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include "actions.h"
#include "config.h"
#include "net.h"
#include "ota.h"
#include "status_led.h"

static QueueHandle_t queue;

static void send(Gesture gesture) {
    const char* action = actionFor(gesture);
    if (!action) return;
    if (otaActive()) {
        Serial.printf("Drop %s: OTA in progress\n", action);
        return;
    }
    if (!netConnected()) {
        Serial.printf("Drop %s: WiFi down\n", action);
        statusLedFlash(Flash::Fail);
        return;
    }

    String url = "http://" + netServerIp().toString() + ":" + String(SERVER_PORT) +
                 "/api/buttons/press/" + action;
    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);

    uint32_t start = millis();
    int code = -1;
    if (http.begin(url)) {
        code = http.POST("");
        http.end();
    }
    Serial.printf("POST %s -> %d (%lu ms)\n", action, code, (unsigned long)(millis() - start));

    if (code >= 200 && code < 300) {
        statusLedFlash(Flash::Ok);
        return;
    }
    statusLedFlash(Flash::Fail);
    // No retry: a repeated start or claps on stage is worse than a miss.
    // A negative code means no connection; the Pi may have a new address.
    if (code < 0) netForgetServerIp();
}

static void senderTask(void*) {
    Gesture gesture;
    for (;;) {
        if (xQueueReceive(queue, &gesture, portMAX_DELAY) == pdTRUE) send(gesture);
    }
}

void senderBegin() {
    queue = xQueueCreate(GESTURE_QUEUE_LEN, sizeof(Gesture));
    xTaskCreate(senderTask, "sender", 8192, nullptr, 1, nullptr);
}

bool senderEnqueue(Gesture gesture) {
    return xQueueSend(queue, &gesture, 0) == pdTRUE;
}
