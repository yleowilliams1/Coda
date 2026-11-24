#include <stdlib.h>
#include <string.h>
#include "util/gild_basic.h"
#include "util/gild_log.h"
#include "util/gild_string.h"
#include "map.h"
#include "entity.h"
#include "save.h"
#include "util/gild_mem.h"
#include "game.h"

// Reads the map's own .ments (save 0) or game save `save`.
static bool read_entities(struct EntityList *out, const char *path, u32 save){
	if(save > 0){
		if(!read_entity_save(out, path, save)){return false;} // already logged why
		GILD_LOG(GILD_INFO, "Loaded save %u of %s", save, path);
		return true;
	}
	char *epath = map_entity_path(path);
	bool ok = epath && read_map_entities(out, epath);
	free(epath);
	return ok;
}

// First load of an entity: its state comes from the prototype. No permanent upgrades yet.
static void fill_from_prototype(struct Entity *e, const struct EntityPrototype *proto){
	memset(&e->actual_stats, 0, sizeof(e->actual_stats));
	e->actual_inventory = proto->starting_inventory;
	e->actual_level = proto->starting_level;
	e->actual_flags = proto->flags;
	e->initialised = true;
}

// Loads the map at path and its entities (see read_entities), and the prototype of
// every NPC/PLAYER entity that names one. Entities that haven't been initialised
// yet are filled from their prototype.
// On failure returns false and leaves out untouched. On success out is
// overwritten (not freed first) and owns everything in it: free with free_game_data.
bool load_map(const char *path, u32 save, struct GameData *out){
	if(!path || !out){GILD_LOG(GILD_IS_NULL, "Passed NULL path or output"); return false;}

	struct Map *m = GILD_XCALLOC(1, sizeof(*m));
	bool ok = read_map(m, path) && read_entities(&m->entities, path, save);
	if(!ok){free_map(m); free(m); return false;}

	// Too big for the stack.
	struct GameData *g = GILD_XCALLOC(1, sizeof(*g));
	g->m = m;
	g->map_path = gild_copy_alloc_str(path, strlen(path));
	for(u16 i = 0; i < m->entities.count; i++){
		struct Entity *e = &m->entities.items[i];
		if((e->type != NPC && e->type != PLAYER) || !e->prototype){continue;}
		// proto and proto_uids are filled in lockstep, so one check covers both.
		if(g->active_proto >= ESIZE){
			GILD_LOG(GILD_FULL, "More than %d entities in %s, skipping the rest", ESIZE, path);
			break;
		}

		struct EntityPrototype *buff = open_prototype_file(e->prototype, PROTO_MAGIC, PROTO_VERSION);
		if(!buff){continue;} // already logged by open_prototype_file. Stays uninitialised until it loads.

		struct EntityPrototype *proto = &g->proto[g->active_proto];
		*proto = *buff; // move: the copy now owns dialogue_path, so only the shell is freed
		free(buff);
		g->proto_uids[g->active_proto++] = e->uid;

		if(!e->initialised){fill_from_prototype(e, proto);}
	}

	*out = *g; // the map pointer and all strings move with the copy
	free(g);
	return true;
}

bool save_entities(struct GameData *g){
	if(!g || !g->m || !g->map_path){GILD_LOG(GILD_IS_NULL, "Passed NULL or unloaded game data"); return false;}
	return write_entity_save(&g->m->entities, g->map_path);
}

const struct EntityPrototype *entity_prototype(const struct GameData *g, u32 uid){
	if(!g){return NULL;}
	for(u8 i = 0; i < g->active_proto; i++){
		if(g->proto_uids[i] == uid){return &g->proto[i];}
	}
	return NULL;
}

// Frees what g owns (the map, its entities, the prototypes and the map path), not g itself.
void free_game_data(struct GameData *g){
	if(!g){return;}
	for(u8 i = 0; i < g->active_proto; i++){free_entity_prototype(&g->proto[i]);}
	g->active_proto = 0;
	free(g->map_path);
	g->map_path = NULL;
	free_map(g->m);
	free(g->m);
	g->m = NULL;
}
