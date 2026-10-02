#pragma once
#include <stdint.h>

// Polls the Pi's player state in a background task.
void showStateBegin();
// True while music is playing. Unknown or unreachable counts as idle.
bool showPlaying(uint32_t nowMs);
// Treat the show as playing for a few seconds after a start is sent, so a quick
// follow-up gesture is not blocked while the next poll catches up.
void showAssumePlaying(uint32_t nowMs);
