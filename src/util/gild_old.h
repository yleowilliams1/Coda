#pragma once
#include <stdio.h>
#include "gild_basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GILD_MAP_NAME_MAX 16

struct Gild_MapSlot{
	u64  hash;
	char name[GILD_MAP_NAME_MAX]; 
	void *data;
};

struct Gild_HashMap{
	
	void *arena;
	char *
	u32 entry_count;          // maximum number of entries
	u32 actual_entry_count;   // entries currently stored
	u32 array_size;           // power of 2, always > entry_count
};

GILD_API struct Gild_HashMap *gild_map_make(const u32 entry_count);
GILD_API bool gild_map_add(struct Gild_HashMap *m, void *data, const char *name);
GILD_API bool gild_map_get(struct Gild_HashMap *m, void *out, const char *name);
GILD_API void gild_map_free(struct Gild_HashMap *m);
GILD_API u64  gild_murmurhash64a(const void *key, size_t len, u64 seed);

typedef bool  (*Gild_MapValueWriter)(FILE *f, const void *data, void *user);
typedef void *(*Gild_MapValueReader)(FILE *f, void *user);

GILD_API bool gild_map_save(const struct Gild_HashMap *m, const char *path, const char magic[8], u32 version, Gild_MapValueWriter write_value, void *user);
GILD_API struct Gild_HashMap *gild_map_load(const char *path,const char magic[8], u32 version,Gild_MapValueReader read_value, void *user);

GILD_API bool gild_map_write(FILE *f, const struct Gild_HashMap *m,Gild_MapValueWriter write_value, void *user);
GILD_API struct Gild_HashMap *gild_map_read(FILE *f, Gild_MapValueReader read_value, void *user);
 
#ifdef __cplusplus
}
#endif
