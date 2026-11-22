#pragma once
#include "util/gild_math.h"
#include "stats.h"
#include "inventory.h"
#define ENTITY_TYPE_LIST\
	X(PLAYER)\
	X(GOBLIN)\
	X(CIVILIAN)

enum EntityFlags {
	ENTITY_FLAG_NONE      = 0u,
	ENTITY_FLAG_DEAD      = 1u << 0,
	ENTITY_FLAG_STATIC    = 1u << 1,
	ENTITY_FLAG_NO_COMBAT = 1u << 2,
	ENTITY_FLAG_HOSTILE   = 1u << 3,
	ENTITY_FLAG_DIALOGUE  = 1u << 4,
};

enum EntityType {
	#define X(name) name##_ENTITY,
	ENTITY_TYPE_LIST
	#undef X
	ENTITY_TYPE_COUNT
};

struct EntityPrototype {
	enum EntityType type;
	struct StatsPrototype stats;
	struct Inventory starting_inventory;
	gild_rec_u32 collision;
	u32 starting_level;
	const char *dialogue_path;
	u8 flags;
};

struct EntitySaveable {
	struct StatsSaveable actual_stats;
	struct Inventory actual_inventory;
	const char *prototype_path;
	gild_v2_u32 tpos;
	u32 actual_level;
	u8 actual_flags;
};
