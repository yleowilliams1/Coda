#pragma once
#include "util/gild_basic.h"
#include "util/gild_math.h"
#include "util/gild_astar.h"
#define STAT_LIST\
	X(Beauty)\
	X(Wisdom)\
	X(Power)\
	X(Fortune)\
	X(Agility)\
	X(Education)\
	X(Perception)\
	X(Social)
#define SKILL_LIST\
	X(Melee)\
	X(Awareness)\
	X(Attraction)\
	X(Luck)\
	X(Science)\
	X(Guns)\
	X(Medicine)\
	X(Persuasion)
#define DERIVED_LIST\
	X(MaxHealth)\
	X(CarryWeight)\
	X(APMod)
enum StatType{
	#define X(name) ST_##name,
	STAT_LIST
	#undef X
	ST_STAT_COUNT
};
enum SkillType{
	#define X(name) ST_##name,
	SKILL_LIST
	#undef X
	ST_SKILL_COUNT
};
enum DerivedType{
	#define X(name) ST_##name,
	DERIVED_LIST
	#undef X
	ST_DERIVED_COUNT
};
struct ItemPrototype{
	s16 use_health_mod_min;
	s16 use_health_mod_max;
	u16 value; 
	u8 effective_skill;
	s8 AC;	
	s8 max_stack;
	s8 bonus;
	
	char *name;
	char *description;
};
// No skills, all prototypes start at level 1. Dialogue is per map not per prototype
struct CharacterPrototype{
	u32 sprite_index;
	gild_rec_s16 collision;
	s8 stat[ST_STAT_COUNT];
	u8 sprite_w; // Cells for sprite sheets
	u8 sprite_h; 
};
struct ItemStack{
	u32 item_uid;
	u32 stack_count;
};
#define ITEM_NO_SKILL 0xFF
#define HOTBAR_SIZE 8
#define INVENTORY_SIZE 32
#define ENTITY_FLAG_LIST\
	X(DEAD,      1u << 0)\
	X(STATIC,    1u << 1)\
	X(NO_COMBAT, 1u << 2)\
	X(HOSTILE,   1u << 3)

enum EntityFlags {
	ENTITY_FLAG_NONE = 0u,
	#define X(name, bit) ENTITY_FLAG_##name = (bit),
	ENTITY_FLAG_LIST
	#undef X
};
struct CharacterInstance{
	u32 unique_id;
	u32 save_counter;

	u32 prototype_id;
	
	u32 current_map;

	u32 tile_x;
	u32 tile_y;

	s16 pixel_offset_from_tile_x;
	s16 pixel_offset_from_tile_y;

	u16 current_health;
	u16 current_xp;
	u16 xp_to_give;

	s8 skill_bonuses[ST_SKILL_COUNT];
	u8 flags;
	u8 level;
	u8 facing_direction;

	struct ItemStack hotbar[HOTBAR_SIZE];
	struct ItemStack inventory[INVENTORY_SIZE];	
	
	// We don't serialize this
	struct Gild_AstarPath path;
};
bool add_item(struct CharacterInstance *c, u32 item, u32 count);
bool remove_item(struct CharacterInstance *c, u32 item, u32 count);
bool equip_item(struct CharacterInstance *c, u32 item);
bool unequip_item(struct CharacterInstance *c, u32 item);

// need to set unique_id and save_counter when reading
bool serialize_character(struct CharacterInstance *c, bool write);
