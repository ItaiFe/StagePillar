#pragma once
#include <string.h>
#include "gesture.h"

// While the show is idle only a single press (start) does anything.
inline bool gestureAllowed(Gesture gesture, bool showPlaying) {
    return showPlaying || gesture == Gesture::Single;
}

// Reads "is_playing" from the body of GET /api/player/state. Anything unexpected
// counts as not playing.
inline bool parseIsPlaying(const char* body) {
    if (!body) return false;
    const char* key = strstr(body, "\"is_playing\"");
    if (!key) return false;
    const char* p = key + strlen("\"is_playing\"");
    while (*p == ' ') p++;
    if (*p != ':') return false;
    p++;
    while (*p == ' ') p++;
    return strncmp(p, "true", 4) == 0;
}
