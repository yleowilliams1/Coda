#pragma once

#include "util/gild_basic.h"
#include "character.h"

struct CharacterInstStorage{
	struct CharacterInstance *instances;
	u32 instance_count;
};
struct CharacterInstList{
	u32 *indexs;
	u32 *versions;
	u32 instance_count;
};
struct CharacterInstList get_character_list();
struct CharacterInstance *get_character_inst(u32 *index, u32 *uid);
void save_character_storage();
void load_character_storage(u32 *uids, u32 *versions, u32 count);
void clear_character_storage();
