#include "net.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include "config.h"

// mDNS itself is started by ArduinoOTA.begin() (see ota.cpp); starting it twice fails.
// Shared by the sender and show-state tasks.
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t cachedIp = 0;  // 0 = not resolved yet

void netBegin() {
    WiFi.setHostname(DEVICE_HOSTNAME);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("WiFi: connecting to %s\n", WIFI_SSID);
}

bool netConnected() {
    return WiFi.isConnected();
}

IPAddress netServerIp() {
    portENTER_CRITICAL(&mux);
    uint32_t cached = cachedIp;
    portEXIT_CRITICAL(&mux);
    if (cached != 0) return IPAddress(cached);

    IPAddress ip = MDNS.queryHost(SERVER_MDNS_NAME, MDNS_TIMEOUT_MS);
    if (uint32_t(ip) != 0) {
        Serial.printf("Server: %s.local is %s\n", SERVER_MDNS_NAME, ip.toString().c_str());
    } else {
        ip.fromString(SERVER_FALLBACK_IP);
        Serial.printf("Server: mDNS lookup failed, using %s\n", SERVER_FALLBACK_IP);
    }
    portENTER_CRITICAL(&mux);
    cachedIp = uint32_t(ip);
    portEXIT_CRITICAL(&mux);
    return ip;
}

void netForgetServerIp() {
    portENTER_CRITICAL(&mux);
    cachedIp = 0;
    portEXIT_CRITICAL(&mux);
}
