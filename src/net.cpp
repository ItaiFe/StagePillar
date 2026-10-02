#include "net.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include "config.h"

// mDNS itself is started by ArduinoOTA.begin() (see ota.cpp); starting it twice fails.
static IPAddress cachedIp;  // 0.0.0.0 = not resolved yet

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
    if (uint32_t(cachedIp) != 0) return cachedIp;

    IPAddress ip = MDNS.queryHost(SERVER_MDNS_NAME, MDNS_TIMEOUT_MS);
    if (uint32_t(ip) != 0) {
        Serial.printf("Server: %s.local is %s\n", SERVER_MDNS_NAME, ip.toString().c_str());
    } else {
        ip.fromString(SERVER_FALLBACK_IP);
        Serial.printf("Server: mDNS lookup failed, using %s\n", SERVER_FALLBACK_IP);
    }
    cachedIp = ip;
    return ip;
}

void netForgetServerIp() {
    cachedIp = IPAddress();
}
