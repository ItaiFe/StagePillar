#pragma once
// Copy to include/secrets.h (gitignored) and fill in real values.

#define WIFI_SSID "Flamingods"
#define WIFI_PASSWORD "change-me"

// Must match the ESP_OTA_PASSWORD env var used by `pio run -e esp32dev-ota -t upload`.
#define OTA_PASSWORD "change-me"

// The StageController Pi, resolved via mDNS as <name>.local.
#define SERVER_MDNS_NAME "flamingods"
#define SERVER_PORT 8000
// Used when the mDNS lookup fails.
#define SERVER_FALLBACK_IP "192.168.0.105"
