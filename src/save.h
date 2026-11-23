#pragma once
#include "util/gild_basic.h"
#include "entity.h"
#include "map.h"
#include "flag.h"

// write_entity/read_entity use "<ENTITY_SAVE_DIR>/<uid>.ent".
#ifndef ENTITY_SAVE_DIR
#define ENTITY_SAVE_DIR "save/entities"
#endif

// Each of these is its own binary file: gild header (magic, timestamp, version),
// then the data. The containing directory must already exist.
//
// Writes check the data before opening the file, so invalid data never truncates
// an existing save.
//
// Reads:
//   - On failure the output is left untouched and nothing is leaked.
//   - On success the output is overwritten. Whatever it held before is NOT freed,
//     so free it first if it owned anything.
//   - Strings that come back (prototype paths) are malloc'd and owned by the struct.

// Map tiles and ceilings. Entities live in their own file (write_map_entities);
// by convention "maps/town.map" goes with "maps/town.ments", see map_entity_path.
// read_map fills w, h, floors, flags, ceilings and ceiling_count, and leaves the
// entities alone.
bool write_map(const struct Map *m, const char *path);
bool read_map(struct Map *out, const char *path);

// The entity file that goes with a map file: the extension is swapped for ".ments"
// (or added if there is none). malloc'd, caller frees.
char *map_entity_path(const char *map_path);

bool write_entity(const struct EntitySaveable *s, u32 uid);
bool read_entity(struct EntitySaveable *out, u32 uid);

bool write_map_entities(const struct Map *m, const char *path);
bool read_map_entities(struct Map *m, const char *path);   // fills m->entities and m->entity_count only

bool write_flags(const struct FlagManager *fm, const char *path);
bool read_flags(struct FlagManager *out, const char *path);

// Only for data whose strings are heap-owned, e.g. anything from the read functions.
// Calling these on string literals will crash.
void free_entity_saveable(struct EntitySaveable *s);
void free_map_entities(struct Map *m);
// Floors, flags, ceilings and entities.
void free_map(struct Map *m);
