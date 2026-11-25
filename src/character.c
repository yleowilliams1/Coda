#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/gild_rand.h"
#include "util/gild_basic.h"
#include "util/gild_io.h"
#include "util/gild_log.h"
#include "util/gild_math.h"
#include "util/gild_astar.h"
#include "map.h"
#include "character.h"
#include "proto.h"
#include "save_version.h"

#define CHARACTER_PATH "data/save/"
#define CHARACTER_MAGIC "CODACHAR" 
#define CHARACTER_VERSION 1
#define MOVE_SPEED    3.0f
#define STEP_STRAIGHT 1.0f 
#define STEP_DIAGONAL 1.41421356f 
#define MAX_DT        0.25f
#define TILE_BLOCKED  (MAP_FLAG_COLLIDE | MAP_FLAG_OCCUPIED) // walls and characters
#define REPATH_INTERVAL 0.5f // seconds between looking for a way round while blocked
#define BLOCKED_GIVE_UP 3.0f // seconds blocked before giving up on the walk
#define MAX_VERSION_SKIPS 100000u // only reached if the counter file was lost and many saves exist

static void io_stack(struct Gild_IO *io, struct ItemStack *s);
static bool character_save_path(u32 uid, u32 counter, char *buf, size_t size);
static bool character_save_exists(u32 uid, u32 counter);
static s32 hotbar_skill_bonus(enum SkillType type, const struct CharacterInstance *c);


static bool tile_blocked(const struct Map *map, u32 x, u32 y);
static void clear_path(struct CharacterInstance *c);
static u8 facing_from(s32 px, s32 py, u8 old);	


static void occupy(struct Map *map, u32 x, u32 y, bool on){
	if(x >= map->w || y >= map->h){return;}
	u8 *f = &map->flags[y * map->w + x];
	*f = on ? (u8)(*f | MAP_FLAG_OCCUPIED) : (u8)(*f & (u8)~MAP_FLAG_OCCUPIED);
}

bool character_place(struct Map *map, struct CharacterInstance *c){
	if(!c || !map || !map->flags){GILD_LOG(GILD_IS_NULL, "Argument is NULL"); return false;}
	if(c->tile_x >= map->w || c->tile_y >= map->h){
		GILD_LOG(GILD_OUT_OF_BOUNDS, "Character %u at %u,%u is off the map", c->unique_id, c->tile_x, c->tile_y);
		return false;
	}
	if(map->flags[c->tile_y * map->w + c->tile_x] & MAP_FLAG_OCCUPIED){
		// One bit per tile: when either of the two leaves, the tile reads as free.
		GILD_LOG(GILD_DUPLICATE, "Character %u placed on occupied tile %u,%u", c->unique_id, c->tile_x, c->tile_y);
	}
	occupy(map, c->tile_x, c->tile_y, true);
	return true;
}

void character_unplace(struct Map *map, struct CharacterInstance *c){
	if(!c || !map || !map->flags){GILD_LOG(GILD_IS_NULL, "Argument is NULL"); return;}
	occupy(map, c->tile_x, c->tile_y, false);
	if(c->stepping){occupy(map, c->step_to.x, c->step_to.y, false);}
	// Whatever it was doing ends here; it's no longer on this map.
	clear_path(c);
	c->stepping = false;
	c->pixel_offset_from_tile_x = 0;
	c->pixel_offset_from_tile_y = 0;
}

void stop_movement(struct CharacterInstance *c){
	if(!c){return;}
	if(c->path.path && c->stepping && c->path_cursor < c->path.path_length){
		c->path.path_length = c->path_cursor + 1; // the tile it's stepping onto is the new end
		return;
	}
	clear_path(c);
}

// The next tile is blocked. Look for a way round to the same goal; if there's none
// (usually someone standing in a corridor), wait and try again every
// REPATH_INTERVAL, giving up after BLOCKED_GIVE_UP. Returns true if there's a new
// path to follow straight away.
static bool handle_blocked(struct Map *map, struct CharacterInstance *c, float dt){
	gild_v2_u32 goal = c->path.path[c->path.path_length - 1];
	c->step_progress = 0.0f; // no saving up movement while standing still

	if(c->blocked_time >= c->next_repath){
		c->next_repath = c->blocked_time + REPATH_INTERVAL;
		if(path_to(map, c, goal.x, goal.y)){
			c->blocked_time = 0.0f;
			c->next_repath = 0.0f;
			return true;
		}
	}
	c->blocked_time += dt;
	if(c->blocked_time >= BLOCKED_GIVE_UP){clear_path(c);}
	return false;
}

void update_movement(struct Map *map, struct CharacterInstance *c, float dt){
	if(!c || !map || !map->flags){GILD_LOG(GILD_IS_NULL, "Argument is NULL"); return;}

	if(!c->path.path){
		// Not walking. A character loaded from a save mid-step just stands on its tile.
		c->pixel_offset_from_tile_x = 0;
		c->pixel_offset_from_tile_y = 0;
		return;
	}
	if(dt < 0.0f){dt = 0.0f;}
	if(dt > MAX_DT){dt = MAX_DT;}
	c->step_progress += MOVE_SPEED * dt;

	// A loop because a slow frame can be enough to finish more than one step.
	for(;;){
		if(!c->stepping){
			if(c->path_cursor >= c->path.path_length){clear_path(c); return;} // arrived

			gild_v2_u32 next = c->path.path[c->path_cursor];
			s32 dx = (s32)next.x - (s32)c->tile_x;
			s32 dy = (s32)next.y - (s32)c->tile_y;
			if(dx == 0 && dy == 0){c->path_cursor++; continue;}
			if(abs(dx) > 1 || abs(dy) > 1){
				GILD_LOG(GILD_INVALID_ARG, "Path jumps from %u,%u to %u,%u", c->tile_x, c->tile_y, next.x, next.y);
				clear_path(c);
				return;
			}

			// Check the next tile before leaving this one (never halfway across).
			// Walls and other characters both count. A diagonal step also needs both
			// tiles beside it clear, the same rule the pathfinder uses, so nobody
			// squeezes between two characters or round a wall corner.
			bool blocked = tile_blocked(map, next.x, next.y);
			if(!blocked && dx != 0 && dy != 0){
				blocked = tile_blocked(map, next.x, c->tile_y) || tile_blocked(map, c->tile_x, next.y);
			}
			if(blocked){
				if(handle_blocked(map, c, dt)){continue;}
				return;
			}

			// Start the step: reserve the tile we're stepping onto.
			occupy(map, next.x, next.y, true);
			c->stepping = true;
			c->step_to = next;
			c->blocked_time = 0.0f;
			c->next_repath = 0.0f;
		}

		s32 dx = (s32)c->step_to.x - (s32)c->tile_x;
		s32 dy = (s32)c->step_to.y - (s32)c->tile_y;
		s32 tx = (dx - dy) * (TILE_W / 2);
		s32 ty = (dx + dy) * (TILE_H / 2);
		c->facing_direction = facing_from(tx, ty, c->facing_direction);

		float step = (dx != 0 && dy != 0) ? STEP_DIAGONAL : STEP_STRAIGHT;
		if(c->step_progress < step){
			// Part way: the offset is the same fraction of the way as the progress.
			c->pixel_offset_from_tile_x = (s16)((float)tx * c->step_progress / step);
			c->pixel_offset_from_tile_y = (s16)((float)ty * c->step_progress / step);
			return;
		}

		// Arrived on the next tile: release the one we left. Leftover progress
		// carries into the next step.
		occupy(map, c->tile_x, c->tile_y, false);
		c->step_progress -= step;
		c->tile_x = c->step_to.x;
		c->tile_y = c->step_to.y;
		c->stepping = false;
		c->pixel_offset_from_tile_x = 0;
		c->pixel_offset_from_tile_y = 0;
		c->path_cursor++;
	}
}

bool is_moving(struct CharacterInstance *c){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return false;}
	return c->path.path != NULL;
}

// Finds a path and starts walking. If the character is part way to a tile, it
// finishes that step first and then follows the new path, so it never jumps.
// If no path is found, the current walk carries on.
// Paths go round walls and around other characters where they stand right now;
// update_movement deals with them moving into the way later.
bool path_to(struct Map *map, struct CharacterInstance *c, u32 tile_x, u32 tile_y){
	if(!c || !map || !map->flags){GILD_LOG(GILD_IS_NULL, "Argument is NULL"); return false;}

	gild_v2_u32 start = {.x = c->tile_x, .y = c->tile_y};
	u32 cursor = 1; // path[0] is the tile we're standing on
	float progress = 0.0f;
	if(c->stepping){
		start = c->step_to; // path from the tile we're stepping onto
		cursor = 0;         // and finish walking onto it first
		progress = c->step_progress;
	}

	// The character's own tiles mustn't block its own path (e.g. turning back
	// mid-step through the tile it's leaving), so lift them during the search.
	u8 *own_a = &map->flags[c->tile_y * map->w + c->tile_x];
	u8 *own_b = c->stepping ? &map->flags[c->step_to.y * map->w + c->step_to.x] : NULL;
	u8 saved_a = *own_a, saved_b = own_b ? *own_b : 0;
	*own_a = (u8)(*own_a & (u8)~MAP_FLAG_OCCUPIED);
	if(own_b){*own_b = (u8)(*own_b & (u8)~MAP_FLAG_OCCUPIED);}

	struct Gild_AstarPath path = {0};
	gild_v2_u32 target = {.x = tile_x, .y = tile_y};
	bool found = gild_pathfind_mask(map->flags, map->w, map->h, start, target, TILE_BLOCKED, &path);

	*own_a = saved_a;
	if(own_b){*own_b = saved_b;}
	if(!found){return false;}

	clear_path(c); // frees the old path: gild_pathfind doesn't
	c->path = path;
	c->path_cursor = cursor;
	c->step_progress = progress;
	c->blocked_time = 0.0f;
	c->next_repath = 0.0f;
	return true;
}
// Total XP needed to reach the next level: 1000 for level 2, 2000 for level 3, ...
static u32 xp_threshold(u8 level){
	return (u32)level * 1000u;
}

// One skill point per level. Points already spent are the skill bonuses.
u32 get_unused_skill_points(struct CharacterInstance *c){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return 0;}
	s32 spent = 0;
	for(int i = 0; i < ST_SKILL_COUNT; i++){
		spent += c->skill_bonuses[i];
	}
	s32 unused = (s32)c->level - spent;
	return unused > 0 ? (u32)unused : 0; // never wraps around to a huge number
}

// Gains every level the character's XP covers. Returns true if it gained any.
bool level_up(struct CharacterInstance *c){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return false;}
	bool gained = false;
	while(c->level < UINT8_MAX && c->current_xp >= xp_threshold(c->level)){
		c->level++;
		gained = true;
	}
	return gained;
}

// XP still needed for the next level, 0 if the character already has enough.
u32 get_xp_to_level(struct CharacterInstance *c){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return 0;}
	u32 need = xp_threshold(c->level);
	return c->current_xp >= need ? 0 : need - c->current_xp;
}
bool target_character(struct CharacterInstance *target, struct CharacterInstance *attacker, u32 item_uid){
	if(!target || !attacker){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return false;}
	if(target->flags & ENTITY_FLAG_DEAD || attacker->flags & ENTITY_FLAG_DEAD){return false;}

	bool found = false;
	for(int i = 0; i < HOTBAR_SIZE; i++){
		if(attacker->hotbar[i].stack_count > 0 && attacker->hotbar[i].item_uid == item_uid){found = true; break;}
	}
	if(!found){return false;}

	const struct ItemPrototype *item = get_item_proto(item_uid);
	const struct CharacterPrototype *target_proto = get_character_proto(target->prototype_id);
	if(!item || !target_proto){return false;}
	if(item->effective_skill == ITEM_NO_SKILL){return false;}

	bool win = skill_check(item->effective_skill, -((int)target_proto->stat[ST_Agility]/4), attacker);
	if(!win){return false;}

	s32 damage = gild_rng(item->use_health_mod_min, item->use_health_mod_max) + get_ac(target);
	if(damage > 0){damage = 0;}
	health_character(target, (s16)damage);
	return true;
}

bool health_character(struct CharacterInstance *c, s16 val){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return false;}
	if(c->flags & ENTITY_FLAG_DEAD){return false;} 

	s32 max = get_derived(ST_MaxHealth, c);
	if(max <= 0){GILD_LOG(GILD_INVALID_ARG, "Character %u has no max health", c->unique_id); return false;}

	s32 hp = (s32)c->current_health + val; 
	if(hp > max){hp = max;}
	if(hp <= 0){
		hp = 0;
		c->flags = (u8)(c->flags | ENTITY_FLAG_DEAD);
	}
	c->current_health = (u16)hp;
	return true;
}
bool skill_check(enum SkillType type, s32 mod, struct CharacterInstance *c){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return 0;}
	s32 roll = gild_rng(1, 6) + gild_rng(1, 6) + gild_rng(1, 6);
	GILD_LOG(GILD_INFO, "Rolled %d", (int)roll);
	
	// critical 
	if(roll == 3 || roll == 4){return true;}

	s32 skill = get_skill(type ,c);

	if((roll == 5 && skill >= 15) || (roll == 6 && skill >= 16)){return true;}

	// critical fail;
	if(roll == 17 || roll == 18){return false;}
	
	roll -= mod;
	return roll <= skill; 
}
s32 get_ac(const struct CharacterInstance *c){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return 0;}
	
	s32 ac = 0;
	bool found = false;
	for(int i = 0; i < HOTBAR_SIZE; i++){
		const struct ItemStack *s = &c->hotbar[i];
		if(s->stack_count == 0){continue;} // empty slot
		const struct ItemPrototype *item = get_item_proto(s->item_uid);
		if(!item){continue;}
		if(item->AC == 0){continue;}
		if(!found || item->AC > ac){ac = item->AC; found = true;}
	}
	return ac;
}
s32 get_skill(enum SkillType type, const struct CharacterInstance *c){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return 0;}
	if((u32)type >= ST_SKILL_COUNT){GILD_LOG(GILD_INVALID_ARG, "Unknown skill %d", (int)type); return 0;}

	const struct CharacterPrototype *proto = get_character_proto(c->prototype_id);
	if(!proto){GILD_LOG(GILD_IS_NULL, "Prototype %u is invalid", c->prototype_id); return 0;}

	s32 base = 0;
	switch(type){
		// Innate
		case ST_Melee:      base = proto->stat[ST_Power]      + SKILL_AVERAGE;   break;
		case ST_Awareness:  base = proto->stat[ST_Wisdom]     + SKILL_AVERAGE;   break;
		case ST_Attraction: base = proto->stat[ST_Beauty]     + SKILL_AVERAGE;   break;
		case ST_Luck:       base = proto->stat[ST_Fortune]    + SKILL_VERY_HARD; break;
		// Earned
		case ST_Science:    base = proto->stat[ST_Education]  + SKILL_HARD;      break;
		case ST_Guns:       base = proto->stat[ST_Perception] + SKILL_HARD;      break;
		case ST_Medicine:   base = proto->stat[ST_Agility]    + SKILL_HARD;      break;
		case ST_Persuasion: base = proto->stat[ST_Social]     + SKILL_HARD;      break;

		case ST_SKILL_COUNT: break; // not a skill, already rejected above
	}
	return base + c->skill_bonuses[type] + hotbar_skill_bonus(type, c);
}

s32 get_derived(enum DerivedType type, const struct CharacterInstance *c){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return 0;}
	if((u32)type >= ST_DERIVED_COUNT){GILD_LOG(GILD_INVALID_ARG, "Unknown derived stat %d", (int)type); return 0;}

	const struct CharacterPrototype *proto = get_character_proto(c->prototype_id);
	if(!proto){GILD_LOG(GILD_IS_NULL, "Prototype %u is invalid", c->prototype_id); return 0;}

	switch(type){
		case ST_MaxHealth:   return proto->stat[ST_Power];
		case ST_CarryWeight: return proto->stat[ST_Power];
		case ST_APMod:       return -(proto->stat[ST_Agility] / 4);

		case ST_DERIVED_COUNT: break; 
	}
	return 0;
}
bool instance_character(u32 uid, u32 save, struct CharacterInstance *out){
	if(!out){return false;}
	if(out->path.path){free(out->path.path); out->path.path = NULL;}
	memset(out, 0, sizeof(*out));
	out->unique_id = uid;
	out->save_counter = save;
	
	if(character_save_exists(uid, save)){
		return serialize_character(out, false); 
	}
	return true;
}	
bool free_character(struct CharacterInstance *c){
	if(!c){return false;}
	if(c->path.path){free(c->path.path); c->path.path = NULL;}
	memset(c, 0, sizeof(*c));
	return true;
}
// Items in the inventory, across every stack.
static u32 inventory_count(const struct CharacterInstance *c, u32 item){
	u64 total = 0;
	for(int i = 0; i < INVENTORY_SIZE; i++){
		if(c->inventory[i].stack_count > 0 && c->inventory[i].item_uid == item){total += c->inventory[i].stack_count;}
	}
	return total > UINT32_MAX ? UINT32_MAX : (u32)total;
}
static bool is_equipped(const struct CharacterInstance *c, u32 item){
	for(int i = 0; i < HOTBAR_SIZE; i++){
		if(c->hotbar[i].stack_count > 0 && c->hotbar[i].item_uid == item){return true;}
	}
	return false;
}

// All or nothing: tops up existing stacks first, then fills empty slots. Returns
// false (and changes nothing) if the whole count doesn't fit.
bool add_item(struct CharacterInstance *c, u32 item, u32 count){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return false;}
	if(count == 0){return true;}
	const struct ItemPrototype *proto = get_item_proto(item);
	if(!proto){GILD_LOG(GILD_NOT_FOUND, "No item prototype %u", item); return false;}
	u32 max = proto->max_stack > 0 ? (u32)proto->max_stack : 1u;

	// First pass: is there room?
	u64 room = 0;
	for(int i = 0; i < INVENTORY_SIZE; i++){
		const struct ItemStack *s = &c->inventory[i];
		if(s->stack_count == 0){room += max;}
		else if(s->item_uid == item && s->stack_count < max){room += max - s->stack_count;}
	}
	if(room < count){return false;}

	// Second pass: place it.
	u32 left = count;
	for(int i = 0; i < INVENTORY_SIZE && left > 0; i++){
		struct ItemStack *s = &c->inventory[i];
		if(s->stack_count > 0 && s->item_uid == item && s->stack_count < max){
			u32 add = max - s->stack_count < left ? max - s->stack_count : left;
			s->stack_count += add;
			left -= add;
		}
	}
	for(int i = 0; i < INVENTORY_SIZE && left > 0; i++){
		struct ItemStack *s = &c->inventory[i];
		if(s->stack_count == 0){
			u32 add = max < left ? max : left;
			s->item_uid = item;
			s->stack_count = add;
			left -= add;
		}
	}
	return true;
}

// All or nothing: false (and changes nothing) if the inventory holds fewer than
// count. Takes from the last stacks first so the earlier ones stay full.
// Only touches the inventory; an equipped item is on the hotbar, not in here.
bool remove_item(struct CharacterInstance *c, u32 item, u32 count){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return false;}
	if(count == 0){return true;}
	if(inventory_count(c, item) < count){return false;}

	u32 left = count;
	for(int i = INVENTORY_SIZE - 1; i >= 0 && left > 0; i--){
		struct ItemStack *s = &c->inventory[i];
		if(s->stack_count == 0 || s->item_uid != item){continue;}
		u32 take = s->stack_count < left ? s->stack_count : left;
		s->stack_count -= take;
		left -= take;
	}
	return true;
}

// Moves one of the item from the inventory to a free hotbar slot. Fails if the
// character doesn't have one, already has one equipped, or the hotbar is full.
bool equip_item(struct CharacterInstance *c, u32 item){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return false;}
	if(!get_item_proto(item)){GILD_LOG(GILD_NOT_FOUND, "No item prototype %u", item); return false;}
	if(is_equipped(c, item)){return false;} // one of each item at a time
	if(inventory_count(c, item) == 0){return false;}

	for(int i = 0; i < HOTBAR_SIZE; i++){
		if(c->hotbar[i].stack_count == 0){
			remove_item(c, item, 1); // can't fail: counted above
			c->hotbar[i].item_uid = item;
			c->hotbar[i].stack_count = 1;
			return true;
		}
	}
	return false; // hotbar full
}

// Moves the item from the hotbar back into the inventory. Fails (and stays
// equipped) if it isn't equipped or the inventory has no room for it.
bool unequip_item(struct CharacterInstance *c, u32 item){
	if(!c){GILD_LOG(GILD_IS_NULL, "Character instance is NULL"); return false;}
	for(int i = 0; i < HOTBAR_SIZE; i++){
		struct ItemStack *s = &c->hotbar[i];
		if(s->stack_count == 0 || s->item_uid != item){continue;}
		if(!add_item(c, item, s->stack_count)){return false;}
		s->stack_count = 0;
		return true;
	}
	return false;
}


bool serialize_character(struct CharacterInstance *c, bool write){
	if(!c){GILD_LOG(GILD_IS_NULL, "Passed NULL character"); return false;}

	// Writing gets a brand-new version from the global counter (save_version.h), so it
	// can never overwrite a file another save slot points at. c->save_counter only
	// moves to it once the file has written cleanly, so a failed save leaves c
	// pointing at its last good file.
	u32 counter = c->save_counter;
	if(write){
		// The counter should never hand out a version whose file exists. This check
		// is the backstop for when it does anyway (counter file deleted, damaged, or
		// copied from another machine): skip forward rather than overwrite.
		u32 tries = 0;
		do{
			if(!save_version_next(&counter)){return false;}
			if(++tries > MAX_VERSION_SKIPS){
				GILD_LOG(GILD_FULL, "Character %u: no free save version found", c->unique_id);
				return false;
			}
		} while(character_save_exists(c->unique_id, counter));
	}

	char path[512];
	if(!character_save_path(c->unique_id, counter, path, sizeof(path))){return false;}

	// Saving reads straight from c. Loading fills a blank copy, and c only changes
	// once the whole file has read cleanly.
	struct CharacterInstance tmp = {0};
	struct CharacterInstance *t = write ? c : &tmp;

	FILE *f = gild_fopen(path, write ? "wb" : "rb");
	if(!f){GILD_LOG(GILD_NO_FILE, "Can't open %s", path); return false;}

	struct Gild_IO io = gild_io_begin(f, write);
	u32 version = CHARACTER_VERSION;
	gild_io_header(&io, CHARACTER_MAGIC, &version, NULL);
	if(version != CHARACTER_VERSION){ // later: read older versions here instead of failing
		GILD_LOG(GILD_VERSION_MISMATCH, "%s is version %u, expected %u", path, version, CHARACTER_VERSION);
		io.ok = false;
	}

	gild_io_u32(&io, &t->unique_id);
	// Writing stores the new counter; reading fills tmp.save_counter.
	u32 file_counter = counter;
	gild_io_u32(&io, &file_counter);
	if(!write){tmp.save_counter = file_counter;}

	gild_io_u32(&io, &t->prototype_id);

	gild_io_u32(&io, &t->current_map);

	gild_io_u32(&io, &t->tile_x);
	gild_io_u32(&io, &t->tile_y);

	gild_io_s16(&io, &t->pixel_offset_from_tile_x);
	gild_io_s16(&io, &t->pixel_offset_from_tile_y);

	gild_io_u16(&io, &t->current_health);
	gild_io_u16(&io, &t->current_xp);
	gild_io_u16(&io, &t->xp_to_give);

	for(u32 i = 0; i < ST_SKILL_COUNT; i++){gild_io_s8(&io, &t->skill_bonuses[i]);}
	gild_io_u8(&io, &t->flags);
	gild_io_u8(&io, &t->level);
	gild_io_u8(&io, &t->facing_direction);

	for(u32 i = 0; i < HOTBAR_SIZE; i++){io_stack(&io, &t->hotbar[i]);}
	for(u32 i = 0; i < INVENTORY_SIZE; i++){io_stack(&io, &t->inventory[i]);}

	// path is runtime only and never saved.

	// Checks on what was loaded. More go here (prototype exists, tile inside the map).
	if(!write && io.ok && (tmp.unique_id != c->unique_id || tmp.save_counter != c->save_counter)){
		GILD_LOG(GILD_CORRUPT, "%s holds character %u.%u", path, tmp.unique_id, tmp.save_counter);
		io.ok = false;
	}

	bool ok = gild_io_end(&io);
	if(fclose(f) != 0){ok = false;}

	if(write){
		if(!ok){remove(path); return false;} // don't leave a half-written save behind
		c->save_counter = counter;
		return true;
	}
	if(!ok){return false;}

	// Loaded: the character stands where the save says, with no walk in progress.
	free(c->path.path);
	*c = tmp;
	return true;
}
static bool character_save_path(u32 uid, u32 counter, char *buf, size_t size){
	int n = snprintf(buf, size, "%s%u.%u.sav", CHARACTER_PATH, uid, counter);
	if(n < 0 || (size_t)n >= size){
		GILD_LOG(GILD_TRUNCATED, "Character save path too long");
		return false;
	}
	return true;
}
static bool character_save_exists(u32 uid, u32 counter){
	char path[512];
	if(!character_save_path(uid, counter, path, sizeof(path))){return false;}
	FILE *f = gild_fopen(path, "rb");
	if(!f){return false;}
	fclose(f);
	return true;
}

static void io_stack(struct Gild_IO *io, struct ItemStack *s){
	gild_io_u32(io, &s->item_uid);
	gild_io_u32(io, &s->stack_count);
}
static s32 hotbar_skill_bonus(enum SkillType type, const struct CharacterInstance *c){
	s32 total = 0;
	for(int i = 0; i < HOTBAR_SIZE; i++){
		const struct ItemStack *s = &c->hotbar[i];
		if(s->stack_count == 0){continue;} // empty slot
		const struct ItemPrototype *item = get_item_proto(s->item_uid);
		if(item && item->effective_skill == type){total += item->bonus;}
	}
	return total;
}
static bool tile_blocked(const struct Map *map, u32 x, u32 y){
	if(x >= map->w || y >= map->h){return true;}
	return (map->flags[y * map->w + x] & TILE_BLOCKED) != 0;
}

static void clear_path(struct CharacterInstance *c){
	free(c->path.path);
	memset(&c->path, 0, sizeof(c->path));
	c->path_cursor = 0;
	c->step_progress = 0.0f;
}

static u8 facing_from(s32 px, s32 py, u8 old){
	bool east  = px > 0 || (px == 0 && (old == DIR_NE || old == DIR_SE));
	bool north = py < 0 || (py == 0 && (old == DIR_NE || old == DIR_NW));
	if(east){return north ? DIR_NE : DIR_SE;}
	return north ? DIR_NW : DIR_SW;
}

