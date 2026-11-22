#pragma once
#include "util/gild_math.h"
#include "util/gild_basic.h"

struct MapEntity{
	const char *path_to_dialogue; 
	const char *path_to_entity;
	const char *path_to_stats;
	const char *path_to_inventory;
};

struct Map{
	struct MapEntity *entities;
	u16 entity_count;
	u16 w, h;
	u8 *flags;
	u8 *doors;
	u8 *floor_sprite;
	u8 *props;	
	u8 *walls;
	u8 *ceiling;

};
