#include <stdlib.h>
#include "util/gild_basic.h"
#include "util/gild_log.h"
#include "util/gild_mem.h"
#include "character.h"
#include "instance.h"

static struct CharacterInstStorage inst = {0};
struct CharacterInstList get_character_list(void){
	struct CharacterInstList list = {0};
	if(!inst.instances || !inst.instance_count){return list;}
	list.instance_count = inst.instance_count;
	list.indexs = GILD_XCALLOC(list.instance_count, sizeof(u32));
	list.versions = GILD_XCALLOC(list.instance_count, sizeof(u32));

	for(u32 i = 0; i < list.instance_count; i++){
		list.indexs[i] = inst.instances[i].unique_id;
		list.versions[i] = inst.instances[i].save_counter;
	}
	return list;
}
struct CharacterInstance *get_character_inst(u32 *index, u32 *uid){
	if(!inst.instances){return NULL;}
	if(index && *index < inst.instance_count){
		return &inst.instances[*index];
	}

	if(uid){
		for(u32 i = 0; i < inst.instance_count; i++){
			if(inst.instances[i].unique_id == *uid){
				return &inst.instances[i];
			}
		}
	}

	return NULL;
}
bool save_character_storage(void){
	bool all_ok = true;
	for(u32 i = 0; i < inst.instance_count; i++){
		if(!serialize_character(&inst.instances[i], true)){
			GILD_LOG(GILD_SAVE, "Character %u failed to save", inst.instances[i].unique_id);
			all_ok = false;
		}
	}
	return all_ok;
}
void load_character_storage(u32 *uids, u32 *versions, u32 count){
	// Always clear first: returning early used to leave the previous map's
	// characters loaded when the new map had none.
	clear_character_storage();
	if(count == 0){return;} // GILD_XCALLOC(0) gives NULL
	if(!uids || !versions){GILD_LOG(GILD_IS_NULL, "Passed NULL uid or version list"); return;}

	inst.instance_count = count;
	inst.instances = GILD_XCALLOC(inst.instance_count, sizeof(struct CharacterInstance));

	for(u32 i = 0; i < inst.instance_count; i++){
		if(!instance_character(uids[i], versions[i], &inst.instances[i])){
			// The instance keeps its uid and counter, with everything else zeroed.
			GILD_LOG(GILD_LOAD, "Character %u version %u failed to load, starting blank", uids[i], versions[i]);
		}
	}
}
void clear_character_storage(void){
	if(!inst.instances){inst.instance_count = 0; return;}
	for(u32 i = 0; i < inst.instance_count; i++){
		free_character(&inst.instances[i]);
	}
	free(inst.instances);
	inst.instances = NULL;
	inst.instance_count = 0;
}
