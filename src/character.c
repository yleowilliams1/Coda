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

#define CHARACTER_PATH "data/save/"
#define CHARACTER_MAGIC "CODACHAR" 
#define CHARACTER_VERSION 1
#define MOVE_SPEED    3.0f
#define STEP_STRAIGHT 1.0f 
#define STEP_DIAGONAL 1.41421356f 
#define MAX_DT        0.25f
#define TILE_BLOCKED  1u

static void io_stack(struct Gild_IO *io, struct ItemStack *s);
static bool character_save_path(u32 uid, u32 counter, char *buf, size_t size);
static bool character_save_exists(u32 uid, u32 counter);
static s32 hotbar_skill_bonus(enum SkillType type, const struct CharacterInstance *c);

// Distnace from two characters
// tile to global position
// Check flags
// Is mosue over characters?
// Draw character(maybe not here consider later)

static bool tile_blocked(const struct Map *map, u32 x, u32 y);
static bool mid_step(const struct CharacterInstance *c);
static void clear_path(struct CharacterInstance *c);
static u8 facing_from(s32 px, s32 py, u8 old);	

void stop_movement(struct CharacterInstance *c){
	if(!c){return;}
	if(c->path.path && mid_step(c) && c->path_cursor < c->path.path_length){
		c->path.path_length = c->path_cursor + 1; // the tile it's stepping onto is the new end
		return;
	}
	clear_path(c);
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

		// Check the next tile before stepping onto it (never halfway across). If it's
		// blocked now, look for a way round to the same destination; no way round, stop.
		if(!mid_step(c) && tile_blocked(map, next.x, next.y)){
			gild_v2_u32 goal = c->path.path[c->path.path_length - 1];
			clear_path(c);
			path_to(map, c, goal.x, goal.y);
			return;
		}

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

		// Arrived on the next tile. Leftover progress carries into the next step.
		c->step_progress -= step;
		c->tile_x = next.x;
		c->tile_y = next.y;
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
bool path_to(struct Map *map, struct CharacterInstance *c, u32 tile_x, u32 tile_y){
	if(!c || !map || !map->flags){GILD_LOG(GILD_IS_NULL, "Argument is NULL"); return false;}

	gild_v2_u32 start = {.x = c->tile_x, .y = c->tile_y};
	u32 cursor = 1; // path[0] is the tile we're standing on
	float progress = 0.0f;
	if(c->path.path && mid_step(c) && c->path_cursor < c->path.path_length){
		start = c->path.path[c->path_cursor]; // path from the tile we're stepping onto
		cursor = 0;                           // and finish walking onto it first
		progress = c->step_progress;
	}

	struct Gild_AstarPath path = {0};
	gild_v2_u32 target = {.x = tile_x, .y = tile_y};
	if(!gild_pathfind(map->flags, map->w, map->h, start, target, &path)){return false;}

	clear_path(c); // frees the old path: gild_pathfind doesn't
	c->path = path;
	c->path_cursor = cursor;
	c->step_progress = progress;
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
bool add_item(struct CharacterInstance *c, u32 item, u32 count){
	if(!c){return false;}
	const struct ItemPrototype *proto = get_item_proto(item);
	if(!proto){return false;}
	u64 max = (u64)proto->max_stack;

	int empty = -1;
	for(int i = 0; i < INVENTORY_SIZE; i++){
		struct ItemStack *s = &c->inventory[i];
		if(s->stack_count == 0){
			if(empty < 0){empty = i;}
			continue;
		}
		if(s->item_uid == item){
			if((u64)s->stack_count + count > max){return false;} // would go over max_stack
			s->stack_count += count;
			return true;
		}
	}
	if(empty < 0 || count > max){return false;} // inventory full, or too many for one stack
	c->inventory[empty].item_uid = item;
	c->inventory[empty].stack_count = count;
	return true;
}
bool remove_item(struct CharacterInstance *c, u32 item, u32 count){
	if(!c){return false;}
	for(int i = 0; i < INVENTORY_SIZE; i++){
		if(c->inventory[i].stack_count > 0 && c->inventory[i].item_uid == item){
			if(c->inventory[i].stack_count < count){
				c->inventory[i].stack_count = 0;
				return true;
			}
			c->inventory[i].stack_count -= count;
			return true;
		}
	}
	return false;
}
bool equip_item(struct CharacterInstance *c, u32 item){
	if(!c){return false;}
	for(int i = 0 ; i < HOTBAR_SIZE; i++){
		if(c->hotbar[i].stack_count == 0){
			c->hotbar[i].item_uid = item;
			c->hotbar[i].stack_count = 1;
			return true;
		}
	}
	return false;
}
bool unequip_item(struct CharacterInstance *c, u32 item){
	if(!c){return false;}
	bool found = false;
	for(int i = 0 ; i < HOTBAR_SIZE; i++){
		if(c->hotbar[i].stack_count > 0 && c->hotbar[i].item_uid == item){
			c->hotbar[i].stack_count = 0;
			found = true;
		}
	}
	return found;
}


bool serialize_character(struct CharacterInstance *c, bool write){
	if(!c){GILD_LOG(GILD_IS_NULL, "Passed NULL character"); return false;}

	char path[512];
	if(!character_save_path(c->unique_id, c->save_counter, path, sizeof(path))){return false;}

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
	gild_io_u32(&io, &t->save_counter);

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
		if(!ok){remove(path);} // don't leave a half-written save behind
		return ok;
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

static bool mid_step(const struct CharacterInstance *c){
	return c->pixel_offset_from_tile_x != 0 || c->pixel_offset_from_tile_y != 0;
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

