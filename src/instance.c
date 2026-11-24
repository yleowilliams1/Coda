#include <stdlib.h>
#include "util/gild_basic.h"
#include "util/gild_mem.h"
#include "character.h"
#include "instance.h"
#include "map.h"

static struct CharacterInstStorage inst = {0};

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
void init_character_storage(struct Map *m){
	if(!m){return;}
	clear_character_storage();

	inst.instance_count = m->character_count;
	inst.instances = GILD_XCALLOC(inst.instance_count, sizeof(u32));
	for(int i = 0; i < (int)inst.instance_count; i++){
		instance_character(m->characters[i].uid, m->characters[i].save_index, &inst.instances[i]);
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

