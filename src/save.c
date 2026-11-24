#include <stdbool.h>
#include <stdio.h>
#include "util/gild_log.h"
#include "util/gild_basic.h"
#include "util/gild_io.h"
#include "save.h"
#define SAVE_VERSION 1
#define SAVE_MAGIC "SAVEEPIC"
#define SAVE_PATH "data/save_file/"

static struct SaveFile active_save = {0};

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
	for(int i = 0; i < (int)t->instance_count; i++){
		gild_io_u32(&io, &t->instance_versions[i]);
	}

	bool ok = gild_io_end(&io);
	if(fclose(f) != 0){ok = false;}

	if(write){if(!ok){remove(path);}}
	return ok;
}
