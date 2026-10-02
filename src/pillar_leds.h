#pragma once
#include <stdint.h>
#include "gesture.h"

void pillarLedsBegin();
// Starts the sequence for a gesture. Call from loop().
void pillarLedsPlay(Gesture gesture, uint32_t nowMs);
// Flashes red over the current animation. Safe to call from any task.
void pillarLedsFail();
// Renders a frame every 20 ms; dark during OTA. Call from loop().
void pillarLedsUpdate(uint32_t nowMs, bool otaActive, bool showRunning);
