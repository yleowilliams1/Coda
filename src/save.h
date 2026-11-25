#pragma once
#include "util/gild_basic.h"

struct SaveFile{
	u32 flag_version;
	u32 instance_count;
	u32 *instance_versions;
};

struct Map;

bool serialize_save_file(struct SaveFile *s, u32 save_id, bool write);
bool save_from(u32 id);
bool load_from(u32 id);
bool clear_save_file();
bool instance_from_save(struct Map *m);
