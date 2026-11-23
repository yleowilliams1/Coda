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
// Loads the map at path, its .ments file, and an instance for every NPC/PLAYER
// entity with a prototype: its save if it has one, otherwise fresh from the
// prototype. proto[k], instances[k] and instance_uids[k] are the same entity.
// On failure returns false and leaves out untouched. On success out is
// overwritten (not freed first) and owns everything in it: free with free_game_data.
bool load_map(const char *path, struct GameData *out){
	if(!path || !out){GILD_LOG(GILD_IS_NULL, "Passed NULL path or output"); return false;}

	struct Map *m = GILD_XCALLOC(1, sizeof(*m));
	char *epath = map_entity_path(path);
	bool ok = read_map(m, path) && epath && read_map_entities(m, epath);
	free(epath);
	if(!ok){free_map(m); free(m); return false;}

	// Too big for the stack.
	struct GameData *g = GILD_XCALLOC(1, sizeof(*g));
	g->m = m;
	for(u16 i = 0; i < m->entity_count; i++){
		const struct MapEntity *e = &m->entities[i];
		if((e->type != NPC && e->type != PLAYER) || !e->prototype){continue;}
		// proto, instances and instance_uids are filled in lockstep, so one check covers all three.
		if(g->active_proto >= ESIZE){
			GILD_LOG(GILD_FULL, "More than %d entities in %s, skipping the rest", ESIZE, path);
			break;
		}

		struct EntityPrototype *buff = open_prototype_file(e->prototype, PROTO_MAGIC, PROTO_VERSION);
		if(!buff){continue;} // already logged by open_prototype_file

		struct EntityPrototype *proto = &g->proto[g->active_proto++];
		*proto = *buff; // move: the copy now owns dialogue_path, so only the shell is freed
		free(buff);

		struct EntitySaveable *inst = &g->instances[g->active_instances];
		bool loaded = false;
		if(entity_save_exists(e->uid)){
			loaded = read_entity(inst, e->uid);
			// read_entity has logged why. Starting fresh keeps the game going, but the
			// next save will overwrite the broken file.
			if(!loaded){GILD_LOG(GILD_CORRUPT, "Save for entity %u is unreadable, starting it fresh", e->uid);}
		}
		if(!loaded){
			// Fresh: no save yet, so start from the prototype and the map.
			memset(inst, 0, sizeof(*inst)); // no permanent upgrades
			inst->actual_inventory = proto->starting_inventory;
			inst->actual_level = proto->starting_level;
			inst->actual_flags = proto->flags;
			inst->prototype_path = gild_copy_alloc_str(e->prototype, strlen(e->prototype));
			s32 tx, ty;
			map_world_to_tile(m, (float)e->pos.x, (float)e->pos.y, &tx, &ty);
			inst->tpos.x = gild_safe_cast(u32, tx); // logs if the entity is off the map
			inst->tpos.y = gild_safe_cast(u32, ty);
		}
		g->instance_uids[g->active_instances++] = e->uid;
	}

	*out = *g; // the map pointer and all strings move with the copy
	free(g);
	return true;
}

// Frees what g owns (the map and everything in the arrays), not g itself.
void free_game_data(struct GameData *g){
	if(!g){return;}
	for(u8 i = 0; i < g->active_proto; i++){free_entity_prototype(&g->proto[i]);}
	for(u8 i = 0; i < g->active_instances; i++){free_entity_saveable(&g->instances[i]);}
	g->active_proto = 0;
	g->active_instances = 0;
	free_map(g->m);
	free(g->m);
	g->m = NULL;
}
