#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/gild_rand.h"
#include "util/gild_basic.h"
#include "util/gild_io.h"
#include "util/gild_log.h"
#include "character.h"
#include "proto.h"

#define CHARACTER_PATH "data/save/"
#define CHARACTER_MAGIC "CODACHAR" 
#define CHARACTER_VERSION 1

static void io_stack(struct Gild_IO *io, struct ItemStack *s);
static bool character_save_path(u32 uid, u32 counter, char *buf, size_t size);
static bool character_save_exists(u32 uid, u32 counter);
static s32 hotbar_skill_bonus(enum SkillType type, const struct CharacterInstance *c);

// Gain Xp and level up.
// Check how many skill points you get based on level
// How much xp to next level
// Walk to tile
// Update movement offset
// Is moving?
// Get facing direction
// Distnace from two characters
// tile to global position
// Check flags
// Is mosue over characters?
// Draw character(maybe not here consider later)

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
	if(item->effective_skill == ITEM_NO_SKILL){return false;} // not a weapon (an apple)

	bool win = skill_check(item->effective_skill, -((int)target_proto->stat[ST_Agility]/4), attacker);
	if(!win){return false;}

	// Weapon damage is a negative health change (e.g. -6 to -2). The target's armour
	// soaks some of it, but can never turn a hit into healing.
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
	// No default on purpose: add a skill to SKILL_LIST and forget it here,
	// and the compiler warns you.
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

	// Look through every slot for the existing stack before using an empty one,
	// so an item never ends up in two stacks.
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
