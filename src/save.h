#pragma once
#include "util/gild_basic.h"

struct SaveFile{
	u32 flag_version;
	u32 instance_count;
	u32 *instance_versions; // indexed by character uid; 0 = use the map's version
};

struct Map;
bool create_new_savefile(void);
// Writing: the old file is only replaced once the new one has written cleanly.
// Reading: *s is only replaced (old table freed) once the whole file read cleanly.
bool serialize_save_file(struct SaveFile *s, u32 save_id, bool write);
// Saves every loaded character, then the slot. If a character fails, the slot is left alone.
bool save_from(u32 id);
// On failure the active save is unchanged.
bool load_from(u32 id);
bool clear_save_file(void);
bool instance_from_save(struct Map *m);
