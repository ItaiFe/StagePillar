#pragma once

// Starts ArduinoOTA (and mDNS) the first time WiFi is up, then services it.
// Call from loop().
void otaLoop(bool wifiConnected);
// True while an OTA upload is in progress.
bool otaActive();
