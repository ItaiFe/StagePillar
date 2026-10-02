#pragma once
#include "gesture.h"

// Creates the gesture queue and the task that POSTs gestures to the server.
void senderBegin();
// Non-blocking. Returns false if the queue is full and the gesture was dropped.
bool senderEnqueue(Gesture gesture);
