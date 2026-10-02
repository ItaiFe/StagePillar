#pragma once
#include <IPAddress.h>
#include <stdint.h>
#include "pillar_player.h"

// Uploaded LED plans on LittleFS, per the StageController pillar LED contract.
// Downloads run on the show-state task into staging files; loop() swaps them in,
// so the renderer never reads a file that is being written.

void planStoreBegin();

// --- loop() side ---
// The stored plan for a slot, or the code default when none is stored or it is invalid.
Plan planStoreLookup(Slot slot);
// Swaps in a completed download round. True when plans changed.
bool planStoreApplyPending();
// A downloaded preview ready to play.
bool planStoreTakePreview(Plan& out);
// The server ended the preview.
bool planStoreTakePreviewEnd();

// --- show-state task side ---
// Version of the plans in use (0 = code defaults), reported to the server.
uint32_t planStoreVersion();
// Brings stored plans to `version` and handles the preview id; call after each poll.
void planStoreSync(IPAddress server, uint32_t version, uint32_t previewId);
