#pragma once
#include <stdint.h>
#include "led_pattern.h"

void statusLedBegin();
// Starts a flash pattern. Safe to call from any task.
void statusLedFlash(Flash flash);
// Renders the current pattern. Call from loop().
void statusLedUpdate(uint32_t nowMs, bool wifiConnected);
