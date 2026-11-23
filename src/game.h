#pragma once
#include "util/gild_basic.h"
#include "entity.h"
#include "map.h"

#define ESIZE 32
struct GameData{
	struct Map *m;
	struct EntityPrototype proto[ESIZE];
	u8 active_proto;

	struct EntitySaveable instances[ESIZE];
	u32 instance_uids[ESIZE];
	u8 active_instances;
};

bool load_map(const char *path, struct GameData *out);
void free_game_data(struct GameData *g);
