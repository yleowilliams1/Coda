#include <stdio.h>
#include "util/gild_basic.h"
#include "util/gild_io.h"
#include "util/gild_log.h"
#include "save_version.h"

#define VERSION_PATH  "data/save/version.sav"
#define VERSION_TMP   "data/save/version.sav.tmp"
#define VERSION_MAGIC "CODAVERS"
#define VERSION_FILE_VERSION 1
// Versions are reserved this many at a time, so the counter file is written once
// per block instead of once per character save. A crash wastes at most one block.
#define VERSION_BLOCK 64u

// next:  the version the next call hands out
// limit: the value stored in the file. Everything below it may already be in use,
//        so the file is always updated BEFORE next reaches it.
static u32 next_version = 0;
static u32 limit = 0;
static bool loaded = false;

// The file holds one u32: the first version that has never been handed out.
static bool read_counter(u32 *out){
	FILE *f = gild_fopen(VERSION_PATH, "rb");
	if(!f){return false;}

	struct Gild_IO io = gild_io_begin(f, false);
	u32 ver = VERSION_FILE_VERSION;
	gild_io_header(&io, VERSION_MAGIC, &ver, NULL);
	if(io.ok && ver != VERSION_FILE_VERSION){
		GILD_LOG(GILD_VERSION_MISMATCH, "%s is version %u, expected %u", VERSION_PATH, ver, VERSION_FILE_VERSION);
		io.ok = false;
	}
	u32 value = 0;
	gild_io_u32(&io, &value);
	bool ok = gild_io_end(&io);
	fclose(f);
	if(!ok){return false;}
	*out = value;
	return true;
}

// Temp file then rename, so a failed write never loses the old counter.
static bool write_counter(u32 value){
	FILE *f = gild_fopen(VERSION_TMP, "wb");
	if(!f){GILD_LOG(GILD_NO_FILE, "Can't open %s", VERSION_TMP); return false;}

	struct Gild_IO io = gild_io_begin(f, true);
	u32 ver = VERSION_FILE_VERSION;
	gild_io_header(&io, VERSION_MAGIC, &ver, NULL);
	gild_io_u32(&io, &value);
	bool ok = gild_io_end(&io);
	if(fclose(f) != 0){ok = false;}
	if(!ok){remove(VERSION_TMP); return false;}

	// rename replaces the old file on POSIX but fails on Windows if it exists.
	if(rename(VERSION_TMP, VERSION_PATH) != 0){
		remove(VERSION_PATH);
		if(rename(VERSION_TMP, VERSION_PATH) != 0){
			GILD_LOG(GILD_WRITE, "Can't move %s to %s", VERSION_TMP, VERSION_PATH);
			remove(VERSION_TMP);
			return false;
		}
	}
	return true;
}

static void load(void){
	u32 stored;
	if(read_counter(&stored) && stored >= SAVE_VERSION_FIRST){
		next_version = limit = stored;
	} else {
		// No file (first save ever) or a damaged one. Starting low is still safe:
		// serialize_character skips any version whose file already exists.
		FILE *f = gild_fopen(VERSION_PATH, "rb");
		if(f){
			fclose(f);
			GILD_LOG(GILD_CORRUPT, "%s is damaged, starting the counter again at %u", VERSION_PATH, SAVE_VERSION_FIRST);
		}
		next_version = limit = SAVE_VERSION_FIRST;
	}
	loaded = true;
}

bool save_version_next(u32 *out){
	if(!out){GILD_LOG(GILD_IS_NULL, "Passed NULL out"); return false;}
	if(!loaded){load();}

	if(next_version >= limit){
		if(limit > UINT32_MAX - VERSION_BLOCK){
			GILD_LOG(GILD_OVERFLOW, "Save versions have run out");
			return false;
		}
		// Reserve the next block on disk before handing any of it out.
		if(!write_counter(limit + VERSION_BLOCK)){return false;}
		limit += VERSION_BLOCK;
	}
	*out = next_version++;
	return true;
}

void save_version_reload(void){
	loaded = false;
	next_version = limit = 0;
}
