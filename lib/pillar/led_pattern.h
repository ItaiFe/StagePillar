#pragma once
#include <stdint.h>

enum class Flash : uint8_t { None, Ok, Fail };

// Whether the status LED should be lit. A recent flash takes priority over the
// slow WiFi-down blink.
inline bool ledLevel(Flash flash, uint32_t msSinceFlash, bool wifiConnected, uint32_t nowMs) {
    if (flash == Flash::Ok && msSinceFlash < 100) return true;
    if (flash == Flash::Fail && msSinceFlash < 360) return (msSinceFlash / 60) % 2 == 0;
    if (!wifiConnected) return (nowMs / 500) % 2 == 0;
    return false;
}
