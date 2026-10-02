#pragma once
#include <string.h>
#include "gesture.h"

// While the show is idle only a single press (start) does anything.
inline bool gestureAllowed(Gesture gesture, bool showPlaying) {
    return showPlaying || gesture == Gesture::Single;
}

// Reads GET /api/player/state: the show runs while a song is loaded (playing or
// paused), i.e. "current_song" is not null. Anything unexpected counts as not running.
inline bool parseShowRunning(const char* body) {
    if (!body) return false;
    const char* key = strstr(body, "\"current_song\"");
    if (!key) return false;
    const char* p = key + strlen("\"current_song\"");
    while (*p == ' ') p++;
    if (*p != ':') return false;
    p++;
    while (*p == ' ') p++;
    return *p != '\0' && strncmp(p, "null", 4) != 0;
}
