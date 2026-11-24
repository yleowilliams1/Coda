#pragma once
#include "util/gild_math.h"
#include "util/gild_basic.h"

#define TILE_W 64
#define TILE_H 32
#define TEX_ORIGIN_X 32
#define TEX_ORIGIN_Y 80

#define MAP_MAX_TILES (1u << 22)
#define MAP_FLAG_COLLIDE (1u << 0)

struct MapCeiling {
	u16 x, y, w, h;
	u8 sprite;
};
struct MapTransition {
	u16 x, y, w, h;
	u32 target_id;
	gild_v2_u32 target_pos; 
};
struct MapCharacter{
	u32 uid;
	u32 save_index;
};
struct Map{
	u16 w, h;
	
	u32 *floors;
	u32 *props;

	u32 map_id;
	
	struct MapCharacter *characters;
	u32 character_count;

	struct MapCeiling *ceilings;
	u16 ceiling_count;

	struct MapTransition *transitions;
	u16 transition_count;

	u8 *flags;
};

