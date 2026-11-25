#include <stdlib.h>
#include <string.h>
#include "util/gild_basic.h"
#include "util/gild_mem.h"
#include "character.h"
#include "instance.h"
#include "map.h"

static struct CharacterInstStorage inst = {0};
struct CharacterInstList get_character_list(){
	struct CharacterInstList list = {0};
	if(!inst.instances || !inst.instance_count){return list;}
	inst.instance_count = list.instance_count;
	list.indexs = GILD_XCALLOC(list.instance_count, sizeof(u32));
	list.versions = GILD_XCALLOC(list.instance_count, sizeof(u32));
	memcpy(list.indexs, inst.instances, list.instance_count);

	for(int i = 0; i < (int)list.instance_count; i++){
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
		for(int i= 0; i < (int)inst.instance_count; i++){
			if(inst.instances[i].unique_id == *uid){
				return &inst.instances[i];
			}
		}
	}

	return NULL;
}
void save_character_storage(){
	for(int i = 0; i < (int)inst.instance_count; i++){
		serialize_character(&inst.instances[i], true);
	}
}
void load_character_storage(u32 *uids, u32 *versions, u32 count){
	if(!uids || !versions){return;}
	clear_character_storage();

	inst.instance_count = count;
	inst.instances = GILD_XCALLOC(inst.instance_count, sizeof(u32));
	
	for(int i = 0; i < (int)inst.instance_count; i++){
		instance_character(uids[i], versions[i], &inst.instances[i]);
	}
}
void clear_character_storage(){
	if(!inst.instances){inst.instance_count = 0;}
	for(int i = 0; i < (int)inst.instance_count; i++){
		free_character(&inst.instances[i]);
	}
	free(inst.instances);
	inst.instances = NULL;
}

