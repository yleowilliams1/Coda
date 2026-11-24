#pragma once
#include "util/gild_basic.h"

struct SaveFile{
	u32 instance_count;
	u32 *instance_versions;
};

bool serialize_save_file(struct SaveFile *s, u32 save_id, bool write);
