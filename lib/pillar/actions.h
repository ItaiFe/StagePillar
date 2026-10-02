#pragma once
#include "gesture.h"

// Server action for each gesture, sent as POST /api/buttons/press/<action>.
// Returns nullptr for gestures that send nothing.
inline const char* actionFor(Gesture g) {
    switch (g) {
        case Gesture::Single: return "start";
        case Gesture::Double: return "claps";
        case Gesture::Triple: return "special";
        case Gesture::Quad:   return "skip";
        case Gesture::Long:   return "stop";
        default:              return nullptr;
    }
}
