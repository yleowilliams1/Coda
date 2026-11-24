#pragma once

#include "util/gild_basic.h"
#include "character.h"

struct CharacterInstStorage{
	struct CharacterInstance *instances;
	u32 instance_count;
};

struct CharacterInstance *get_character_inst(u32 *index, u32 *uid);
void save_character_storage();
void init_character_storage(struct Map *m);
void clear_character_storage();
