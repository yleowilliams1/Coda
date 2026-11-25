#pragma once
#include "util/gild_basic.h"

// Global save version numbers.
//
// Character files are "<uid>.<version>.sav". Every version handed out here is new:
// the counter only goes up, and it lives in its own file (data/save/version.sav)
// that no save slot owns, so loading an old slot can't wind it back. Two slots can
// never end up pointing at the same file with different contents.
//
// Versions below SAVE_VERSION_FIRST are never handed out: they're for character
// files you author by hand and reference from a map (MapCharacter.save_index).
// 0 means "no file, start blank".
#define SAVE_VERSION_FIRST 1000u

// Hands out the next unused version in *out. Loads the counter file the first time.
// False only if the counter file can't be written or the numbers have run out.
bool save_version_next(u32 *out);

// Forgets the in-memory counter so the next call re-reads the file. Only needed if
// something outside the game replaced the file (tests, a "delete all saves" option).
void save_version_reload(void);
