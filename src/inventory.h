#pragma once

#include "util/gild_basic.h"
#include "stats.h"
#include <stdbool.h>
#define HOTBAR_SIZE 8
#define INVENTORY_SIZE 32

#define ITEM_LIST\
	X(APPLE)\
	X(DAGGER)\
	X(BUTCHERS_KNIFE)\
	X(LEATHER_JACKET)\
	X(POWER_ARMOR)
enum ItemType{
	#define X(name) ITEM_##name,
	ITEM_LIST
	#undef X
	ITEM_COUNT
};
struct InventoryBonusMatrix{
	s8 stats[ST_STAT_COUNT];
	s8 skills[SK_SKILL_COUNT];
	s8 derived[DE_DERIVED_COUNT];
};
struct ItemPrototype{
	struct InventoryBonusMatrix matrix; 
	u32 value;
	u32 min_dmg;
	u32 max_dmg;
	enum SkillsEnum attack_skill_use;
	u16 weight;
	u16 heal_min;
	u16 heal_max;
	s16 AC;
	char *name;
	char *description;
};
struct ItemStack{
	u32 id;
	u32 count;
	bool active;
};
struct Inventory{
	struct ItemStack hotbar_items[HOTBAR_SIZE];
	struct ItemStack items[INVENTORY_SIZE]; 
};

void init_item_protos();
struct ItemPrototype *get_item_proto(enum ItemType type);
