#pragma once
#include "util/gild_basic.h"
#include "entity.h"
#include "map.h"

#define ESIZE 32
struct GameData{
	struct Map *m;       // m->entities are the entities, prototype-filled and saved as they are
	char *map_path;      // the map this was loaded from; save_entities names its files after it

	// Prototypes of the NPC/PLAYER entities. proto[k] belongs to the entity whose uid
	// is proto_uids[k]. Looked up by uid because draw_map reorders m->entities.items.
	struct EntityPrototype proto[ESIZE];
	u32 proto_uids[ESIZE];
	u8 active_proto;
};

// save 0 loads the map's own .ments; any other number loads that game save
// ("<save>.<map name>.ments", see map_entity_save_path) and fails if it can't be read.
bool load_map(const char *path, u32 save, struct GameData *out);
// Writes the next game save: adds one to g->m->entities.save_index and writes the file
// for that number, see write_entity_save. The directory must already exist.
bool save_entities(struct GameData *g);
// Prototype of the entity with this uid, or NULL if it has none loaded.
const struct EntityPrototype *entity_prototype(const struct GameData *g, u32 uid);
void free_game_data(struct GameData *g);
