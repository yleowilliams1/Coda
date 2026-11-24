#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "util/gild_basic.h"

#define MAX_FLAG_NAME 32

struct Flag {
	char name[MAX_FLAG_NAME];
	u32 hash;
	u8 value;
};

struct FlagManager {
	u32 count;
	u32 capacity;
	u32 flag_version;
	struct Flag *flags;
};
// Set fm->flag_version first: it picks the file (data/save/flag_<version>.sav).
bool serialize_flag(struct FlagManager *fm, bool write);
void flag_set(struct FlagManager *fm, const char *name, bool value);
bool flag_get(struct FlagManager *fm, const char *name);
void flag_free(struct FlagManager *fm);
