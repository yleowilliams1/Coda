#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "util/gild_log.h"
#include "util/gild_basic.h"
#include "util/gild_io.h"
#include "util/gild_mem.h"
#include "save.h"
#include "instance.h"
#include "map.h"
#define SAVE_VERSION 1
#define SAVE_MAGIC "SAVEEPIC"
#define SAVE_PATH "data/save_file/"

static struct SaveFile active_save = {0};
// Do indiviudal bounds check
bool instance_from_save(struct Map *m){
	if(!active_save.instance_versions || !m){return false;}
	u32 *indexes = GILD_XCALLOC(m->character_count, sizeof(u32));
	u32 *versions = GILD_XCALLOC(m->character_count, sizeof(u32));

	for(int i = 0; i < (int)m->character_count; i++){
		indexes[i] = m->characters[i].uid;
		if(active_save.instance_versions[m->characters[i].uid] > m->characters[i].save_index){
			versions[i] = active_save.instance_versions[m->characters[i].uid];
		} else {
		
			versions[i] = m->characters[i].save_index;
		};
	}

	load_character_storage(indexes, versions, m->character_count);

	free(indexes);
	free(versions);
	return true;
}	
bool save_from(u32 id){
	save_character_storage();
	struct CharacterInstList list = get_character_list();
	for(int i = 0; i < (int)list.instance_count; i++){
		active_save.instance_versions[list.indexs[i]] = list.versions[i];
	}
	return serialize_save_file(&active_save, id, true);
	
}
bool load_from(u32 id){
	clear_save_file();
	return serialize_save_file(&active_save, id, true);
}
bool clear_save_file(){
	if(active_save.instance_versions){free(active_save.instance_versions); active_save.instance_versions = NULL;}
	memset(&active_save, 0 , sizeof(active_save));
	return true;
}
bool serialize_save_file(struct SaveFile *s, u32 save_id, bool write){
	if(!s){GILD_LOG(GILD_IS_NULL, "Passed NULL save"); return false;}
	char path[512];
	snprintf(path, sizeof(path), "%s%u.sav", SAVE_PATH, save_id);

	struct SaveFile tmp = {0};
	struct SaveFile *t = write ? s : &tmp;

	FILE *f = gild_fopen(path, write ? "wb" : "rb");
	if(!f){GILD_LOG(GILD_NO_FILE, "Can't open %s", path); return false;}	

	struct Gild_IO io = gild_io_begin(f, write);
	u32 ver = SAVE_VERSION;
	gild_io_header(&io, SAVE_PATH, &ver, NULL);
	if(ver != SAVE_VERSION){ 
		GILD_LOG(GILD_VERSION_MISMATCH, "%s is version %u, expected %u", path, ver, SAVE_MAGIC); 
		io.ok = false;
	}

	gild_io_u32(&io, &t->flag_version);
	gild_io_u32(&io, &t->instance_count);
	if(!write && io.ok){
		t->instance_versions = GILD_XCALLOC(t->instance_count, sizeof(u32)); 
	}
	for(int i = 0; i < (int)t->instance_count; i++){
		gild_io_u32(&io, &t->instance_versions[i]);
	}

	bool ok = gild_io_end(&io);
	if(fclose(f) != 0){ok = false;}

	if(write){if(!ok){remove(path);}}
	return ok;
}
