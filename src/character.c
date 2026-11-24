#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include "util/gild_basic.h"
#include "util/gild_io.h"
#include "util/gild_log.h"
#include "character.h"
#include "proto.h"

#define CHARACTER_PATH "data/save/"
#define CHARACTER_MAGIC "CODACHAR" 
#define CHARACTER_VERSION 1

static void io_stack(struct Gild_IO *io, struct ItemStack *s);

bool add_item(struct CharacterInstance *c, u32 item, u32 count){
	if(!c){return false;}
	for(int i = 0; i < INVENTORY_SIZE; i++){
		if(c->inventory[i].item_uid == item){
			c->inventory[i].stack_count += count;
			return true;
		}
		// found empty slot
		if(c->inventory[i].stack_count == 0){
			c->inventory[i].item_uid = item;
			c->inventory[i].stack_count = count;
			return true;
		}
	}
	return false;
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
	int n = snprintf(path, sizeof(path), "%s%u.%u.sav", CHARACTER_PATH, c->unique_id, c->save_counter);
	if(n < 0 || (size_t)n >= sizeof(path)){GILD_LOG(GILD_TRUNCATED, "Character save path too long"); return false;}

	struct CharacterInstance tmp = {0};
	struct CharacterInstance *t = write ? c : &tmp;

	FILE *f = gild_fopen(path, write ? "wb" : "rb");
	if(!f){GILD_LOG(GILD_NO_FILE, "Can't open %s", path); return false;}

	struct Gild_IO io = gild_io_begin(f, write);
	u32 version = CHARACTER_VERSION;
	gild_io_header(&io, CHARACTER_MAGIC, &version, NULL);
	if(version != CHARACTER_VERSION){ 
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

	if(!write && io.ok && (tmp.unique_id != c->unique_id || tmp.save_counter != c->save_counter)){
		GILD_LOG(GILD_CORRUPT, "%s holds character %u.%u", path, tmp.unique_id, tmp.save_counter);
		io.ok = false;
	}

	bool ok = gild_io_end(&io);
	if(fclose(f) != 0){ok = false;}

	if(write){
		if(!ok){remove(path);} 
		return ok;
	}
	if(!ok){return false;}

	// Loaded: the character stands where the save says, with no walk in progress.
	free(c->path.path);
	*c = tmp;
	return true;
}
static void io_stack(struct Gild_IO *io, struct ItemStack *s){
	gild_io_u32(io, &s->item_uid);
	gild_io_u32(io, &s->stack_count);
}
