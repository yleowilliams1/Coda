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
// Caller frees indexs and versions. Both are NULL when the count is 0.
struct CharacterInstList get_character_list(void);
struct CharacterInstance *get_character_inst(u32 *index, u32 *uid);
// Saves every loaded character. True only if all of them saved; a character that
// failed keeps its old save_counter, so it still points at its last good file.
bool save_character_storage(void);
// count 0 just empties the storage.
void load_character_storage(u32 *uids, u32 *versions, u32 count);
void clear_character_storage(void);
