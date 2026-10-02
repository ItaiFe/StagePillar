#pragma once
#include <IPAddress.h>

// Starts WiFi in the background; it reconnects automatically.
void netBegin();
bool netConnected();
// The Pi's address: an mDNS lookup of SERVER_MDNS_NAME, else SERVER_FALLBACK_IP.
// The result is cached. May block up to MDNS_TIMEOUT_MS; call only from the sender task.
IPAddress netServerIp();
// Forgets the cached address so the next netServerIp() looks it up again.
void netForgetServerIp();
