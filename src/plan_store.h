#pragma once
#include <stdint.h>
#include "pillar_player.h"

// Uploaded LED plans on LittleFS, per the StageController pillar LED contract.
// A background task downloads and validates plans into staging files; loop() only
// renames them into place and takes over the already-built indexes, so it never
// blocks on flash reads or reads a file that is being written.

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

// --- show-state poll side ---
// Version of the plans in use (0 = code defaults), reported to the server.
uint32_t planStoreVersion();
// What the server currently offers; the download task catches up in the background.
void planStoreSetTarget(uint32_t version, uint32_t previewId);
