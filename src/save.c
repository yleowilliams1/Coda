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
#define INSTANCE_COUNT 64
#define MAX_INSTANCES_IN_FILE (1u << 20) // a corrupt count can't make a load allocate gigabytes

static struct SaveFile active_save = {0};

static void free_save(struct SaveFile *s){
	free(s->instance_versions);
	memset(s, 0, sizeof(*s));
}

// Makes sure every uid below INSTANCE_COUNT has a slot, so a save written with a
// smaller table (an older build) still loads. New slots are 0 = "use the map's version".
static void grow_to_instance_count(struct SaveFile *s){
	if(s->instance_count >= INSTANCE_COUNT){return;}
	u32 *grown = GILD_XCALLOC(INSTANCE_COUNT, sizeof(u32));
	if(s->instance_versions){
		memcpy(grown, s->instance_versions, (size_t)s->instance_count * sizeof(u32));
		free(s->instance_versions);
	}
	s->instance_versions = grown;
	s->instance_count = INSTANCE_COUNT;
}

bool create_new_savefile(void){
	clear_save_file();
	active_save.instance_versions = GILD_XCALLOC(INSTANCE_COUNT, sizeof(u32));
	active_save.instance_count = INSTANCE_COUNT;
	return true;
}

bool instance_from_save(struct Map *m){
	if(!m){GILD_LOG(GILD_IS_NULL, "Passed NULL map"); return false;}
	if(!active_save.instance_versions){
		GILD_LOG(GILD_INVALID_STATE, "No save active: call create_new_savefile or load_from first");
		return false;
	}
	// A map with no characters empties the storage, so the last map's don't linger.
	if(m->character_count == 0){clear_character_storage(); return true;}
	if(!m->characters){GILD_LOG(GILD_IS_NULL, "Map claims %u characters but has none", m->character_count); return false;}

	u32 *uids = GILD_XCALLOC(m->character_count, sizeof(u32));
	u32 *versions = GILD_XCALLOC(m->character_count, sizeof(u32));

	for(u32 i = 0; i < m->character_count; i++){
		u32 uid = m->characters[i].uid;
		if(uid >= active_save.instance_count){
			// Left as uid 0, version 0 rather than indexing past the table.
			GILD_LOG(GILD_OUT_OF_BOUNDS, "Character uid %u is over the limit of %u, loading as uid 0",
			         uid, active_save.instance_count - 1);
			continue;
		}
		uids[i] = uid;
		// The save's version wins once the character has been saved past the map's.
		u32 saved = active_save.instance_versions[uid];
		versions[i] = saved > m->characters[i].save_index ? saved : m->characters[i].save_index;
	}

	load_character_storage(uids, versions, m->character_count);

	free(uids);
	free(versions);
	return true;
}

bool save_from(u32 id){
	if(!active_save.instance_versions){
		GILD_LOG(GILD_INVALID_STATE, "No save active: call create_new_savefile or load_from first");
		return false;
	}
	// Characters first. If any fail, the slot isn't touched, so it still points at a
	// complete set of older files instead of a mix that includes ones that don't exist.
	if(!save_character_storage()){
		GILD_LOG(GILD_SAVE, "Slot %u not written: a character failed to save", id);
		return false;
	}

	struct CharacterInstList list = get_character_list();
	for(u32 i = 0; i < list.instance_count; i++){
		u32 uid = list.indexs[i];
		if(uid >= active_save.instance_count){ // used to write past the end of the table
			GILD_LOG(GILD_OUT_OF_BOUNDS, "Character uid %u is over the limit of %u, not recorded",
			         uid, active_save.instance_count - 1);
			continue;
		}
		active_save.instance_versions[uid] = list.versions[i];
	}
	free(list.indexs);
	free(list.versions);
	return serialize_save_file(&active_save, id, true);
}

// On failure the active save is left exactly as it was.
bool load_from(u32 id){
	struct SaveFile loaded = {0};
	if(!serialize_save_file(&loaded, id, false)){return false;}
	clear_save_file();
	active_save = loaded;
	return true;
}

bool clear_save_file(void){
	free_save(&active_save);
	return true;
}

static bool save_path(u32 save_id, const char *suffix, char *buf, size_t size){
	int n = snprintf(buf, size, "%s%u.sav%s", SAVE_PATH, save_id, suffix);
	if(n < 0 || (size_t)n >= size){GILD_LOG(GILD_TRUNCATED, "Save path too long"); return false;}
	return true;
}

// Writing goes to "<id>.sav.tmp" and is renamed over "<id>.sav" only once it has
// written cleanly, so a failed save never destroys the previous one.
// Reading fills a fresh SaveFile and only replaces *s (freeing its old table) once
// the whole file has read cleanly.
bool serialize_save_file(struct SaveFile *s, u32 save_id, bool write){
	if(!s){GILD_LOG(GILD_IS_NULL, "Passed NULL save"); return false;}
	if(write && s->instance_count > 0 && !s->instance_versions){
		GILD_LOG(GILD_IS_NULL, "Save has %u instances but no table", s->instance_count);
		return false;
	}

	char path[512], tmp_path[512];
	if(!save_path(save_id, "", path, sizeof(path)) || !save_path(save_id, ".tmp", tmp_path, sizeof(tmp_path))){return false;}

	FILE *f = gild_fopen(write ? tmp_path : path, write ? "wb" : "rb");
	if(!f){GILD_LOG(GILD_NO_FILE, "Can't open %s", write ? tmp_path : path); return false;}

	struct SaveFile tmp = {0};
	struct SaveFile *t = write ? s : &tmp;

	struct Gild_IO io = gild_io_begin(f, write);
	u32 ver = SAVE_VERSION;
	gild_io_header(&io, SAVE_MAGIC, &ver, NULL);
	if(io.ok && ver != SAVE_VERSION){
		GILD_LOG(GILD_VERSION_MISMATCH, "%s is version %u, expected %u", path, ver, SAVE_VERSION);
		io.ok = false;
	}
	gild_io_u32(&io, &t->flag_version);
	gild_io_u32(&io, &t->instance_count);
	if(!write && io.ok){
		if(t->instance_count > MAX_INSTANCES_IN_FILE){
			GILD_LOG(GILD_CORRUPT, "%s claims %u instances", path, t->instance_count);
			io.ok = false;
		} else if(t->instance_count > 0){ // GILD_XCALLOC(0) gives NULL
			t->instance_versions = GILD_XCALLOC(t->instance_count, sizeof(u32));
		}
	}
	for(u32 i = 0; io.ok && i < t->instance_count; i++){
		gild_io_u32(&io, &t->instance_versions[i]);
	}

	bool ok = gild_io_end(&io);
	if(fclose(f) != 0){ok = false;}

	if(write){
		if(!ok){remove(tmp_path); return false;}
		// rename replaces the old file on POSIX but fails on Windows if it exists.
		if(rename(tmp_path, path) != 0){
			remove(path);
			if(rename(tmp_path, path) != 0){
				GILD_LOG(GILD_WRITE, "Can't move %s to %s", tmp_path, path);
				remove(tmp_path);
				return false;
			}
		}
		return true;
	}

	if(!ok){free_save(&tmp); return false;}
	grow_to_instance_count(&tmp);
	free_save(s);
	*s = tmp;
	return true;
}
