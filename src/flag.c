#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "util/gild_basic.h"
#include "util/gild_log.h"
#include "flag.h"

#define GROW_SIZE 5u

// eventually this needs to become a real hash table like string table but for now this is fine
static u32 flag_hash(const char *name) {
	// DON'T PASS NULL POINTERS.
	u32 hash = 2166136261u;
	while (*name) {
		hash ^= (u8)*name++;
		hash *= 16777619u;
	}
	return hash;
}

static struct Flag *flag_find(struct FlagManager *fm, const char *name, u32 hash) {
	for (u32 i = 0; i < fm->count; i++) {
		if (fm->flags[i].hash == hash && strcmp(fm->flags[i].name, name) == 0) {
			return &fm->flags[i];
		}
	}
	return NULL;
}

void flag_free(struct FlagManager *fm) {
	if (!fm) { return; }
	free(fm->flags);
	*fm = (struct FlagManager){0};
}

void flag_set(struct FlagManager *fm, const char *name, bool value) {
	if (!name) {
		GILD_LOG(GILD_IS_NULL, "Flag name is NULL");
		return;
	}
	if (!fm) {
		GILD_LOG(GILD_IS_NULL, "FlagManager is NULL");
		return;
	}

	size_t len = strlen(name);
	if (len >= MAX_FLAG_NAME) {
		GILD_LOG(GILD_OUT_OF_BOUNDS, "Flag name '%s' exceeds max length %d", name, MAX_FLAG_NAME - 1);
		return;
	}

	u32 hash = flag_hash(name);
	struct Flag *f = flag_find(fm, name, hash);
	if (f) {
		f->value = value ? 1 : 0;
		return;
	}

	if (fm->count >= fm->capacity) {
		u32 new_cap = gild_safe_add(fm->capacity, GROW_SIZE);
		size_t alloc_size = gild_safe_mult((size_t)new_cap, sizeof(struct Flag));
		struct Flag *flags = realloc(fm->flags, alloc_size);
		if (!flags) {
			GILD_LOG(GILD_ALLOC, "Failed to grow FlagManager to %u flags", new_cap);
			return;
		}
		fm->flags = flags;
		fm->capacity = new_cap;
	}

	f = &fm->flags[fm->count];
	fm->count = gild_safe_add(fm->count, 1u);
	f->hash = hash;
	f->value = value ? 1 : 0;
	memcpy(f->name, name, len + 1);
}

bool flag_get(struct FlagManager *fm, const char *name) {
	if (!name) {
		GILD_LOG(GILD_IS_NULL, "Flag name is NULL");
		return false;
	}
	if (!fm) {
		GILD_LOG(GILD_IS_NULL, "FlagManager is NULL");
		return false;
	}

	struct Flag *f = flag_find(fm, name, flag_hash(name));
	return f ? (f->value != 0) : false;
}

