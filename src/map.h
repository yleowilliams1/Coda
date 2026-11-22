#pragma once
#include "util/gild_math.h"
#include "util/gild_basic.h"

#define DL 10
#define DR 11
#define UR 12
#define UL 13

enum EntityType{
	PLAYER,
	NPC,
	DOOR,
	LOOT,
	PROP,
};

struct MapEntity{
	const char *prototype;
	enum EntityType type;
	u32 uid;
	gild_v2_u32 pos;
	u8 sprite;

};
struct MapCeiling {
	u16 x, y, w, h;
	u8 sprite;
};
struct Map{
	struct MapEntity *entities;
	u16 entity_count;
	u16 w, h;

	struct MapCeiling *ceilings;
	u16 ceiling_count;
	
	u8 *flags;
	u8 *floors;
	
	u8 player_state;
	u32 player;
};
