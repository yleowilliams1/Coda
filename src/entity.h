#pragma once
#include "util/gild_math.h"
#include "stats.h"
#include "inventory.h"

#define ENTITY_TYPE_LIST\
	X(PLAYER)\
	X(GOBLIN)\
	X(CIVILIAN)

#define ENTITY_FLAG_LIST\
	X(DEAD,      1u << 0)\
	X(STATIC,    1u << 1)\
	X(NO_COMBAT, 1u << 2)\
	X(HOSTILE,   1u << 3)\
	X(DIALOGUE,  1u << 4)

enum EntityFlags {
	ENTITY_FLAG_NONE = 0u,
	#define X(name, bit) ENTITY_FLAG_##name = (bit),
	ENTITY_FLAG_LIST
	#undef X
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

struct EntityPrototype *open_prototype_file(const char *arg, const char mag[8], u32 ver);
