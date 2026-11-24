#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util/gild_basic.h"
#include "util/gild_log.h"
#include "util/gild_io.h"
#include "flag.h"
#define FLAG_PATH "data/save/flag_%u.sav"
#define FLAG_MAGIC "FLAGEPIC"
#define FLAG_VERSION 1

#define GROW_SIZE 5u
#define MAX_FLAGS_IN_FILE 100000u // a corrupt count can't make a load allocate gigabytes

bool serialize_flag(struct FlagManager *fm, bool write){
	if(!fm){GILD_LOG(GILD_IS_NULL, "Passed NULL flag manager"); return false;}

	char path[512];
	int n = snprintf(path, sizeof(path), FLAG_PATH, fm->flag_version);
	if(n < 0 || (size_t)n >= sizeof(path)){GILD_LOG(GILD_TRUNCATED, "Flag save path too long"); return false;}

	FILE *f = gild_fopen(path, write ? "wb" : "rb");
	if(!f){GILD_LOG(GILD_NO_FILE, "Can't open %s", path); return false;}

	struct Gild_IO io = gild_io_begin(f, write);
	u32 version = FLAG_VERSION;
	gild_io_header(&io, FLAG_MAGIC, &version, NULL);
	if(version != FLAG_VERSION){
		GILD_LOG(GILD_VERSION_MISMATCH, "%s is version %u, expected %u", path, version, FLAG_VERSION);
		io.ok = false;
	}

	// Loading builds a fresh manager; fm only changes once the whole file has read cleanly.
	struct FlagManager tmp = {0};

	u32 count = write ? fm->count : 0;
	gild_io_u32(&io, &count);
	if(!write && io.ok && count > MAX_FLAGS_IN_FILE){
		GILD_LOG(GILD_CORRUPT, "%s claims %u flags", path, count);
		io.ok = false;
	}

	for(u32 i = 0; io.ok && i < count; i++){
		if(write){
			char *name = fm->flags[i].name;
			gild_io_str(&io, &name, MAX_FLAG_NAME - 1);
			gild_io_u8(&io, &fm->flags[i].value);
		} else {
			char *name = NULL;
			u8 value = 0;
			gild_io_str(&io, &name, MAX_FLAG_NAME - 1); // fails if the name is too long
			gild_io_u8(&io, &value);
			if(io.ok && !name){GILD_LOG(GILD_CORRUPT, "%s has a flag with no name", path); io.ok = false;}
			if(io.ok){flag_set(&tmp, name, value != 0);} // works out the hash; a repeated name just overwrites
			free(name);
		}
	}

	bool ok = gild_io_end(&io);
	if(fclose(f) != 0){ok = false;}

	if(write){
		if(!ok){remove(path);} // don't leave a half-written save behind
		return ok;
	}
	if(!ok){flag_free(&tmp); return false;}

	tmp.flag_version = fm->flag_version;
	flag_free(fm);
	*fm = tmp;
	return true;
}

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
