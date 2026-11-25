#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/gild_basic.h"
#include "util/gild_io.h"
#include "util/gild_log.h"
#include "util/gild_mem.h"
#include "map.h"

#define MAP_DIR     "data/maps/"
#define MAP_MAGIC   "CODAMAP_"
#define MAP_VERSION 1

// ---------------------------------------------------------------------------
// File layout (version 1). All integers little-endian, via gild_io.
//
//   header        "CODAMAP_", u64 timestamp, u32 version
//   map_id        u32
//   w, h          u16, u16          1..65535 each, w * h <= MAP_MAX_TILES
//   floors        w*h x u32         row by row: index = y * w + x
//   props         w*h x u32
//   flags         w*h x u8          MAP_FLAG_OCCUPIED is never stored
//   characters    u32 count, then count x { u32 uid, u32 save_index }
//   ceilings      u16 count, then count x { u16 x, y, w, h, u8 sprite }
//   transitions   u16 count, then count x { u16 x, y, w, h, u32 target_id,
//                                           u32 target_pos.x, u32 target_pos.y }
//
// To add a field later: bump MAP_VERSION, add it at the end, and read it only when
// the file's version is new enough, so old maps still load.
// ---------------------------------------------------------------------------

// Flags that are runtime state, never written to a file.
#define MAP_FLAGS_RUNTIME ((u8)MAP_FLAG_OCCUPIED)

void free_map(struct Map *m){
	if(!m){return;}
	free(m->floors);
	free(m->props);
	free(m->flags);
	free(m->characters);
	free(m->ceilings);
	free(m->transitions);
	memset(m, 0, sizeof(*m));
}

bool map_path(u32 map_id, char *buf, size_t size){
	if(!buf){GILD_LOG(GILD_IS_NULL, "Passed NULL buffer"); return false;}
	int n = snprintf(buf, size, "%s%u.map", MAP_DIR, map_id);
	if(n < 0 || (size_t)n >= size){GILD_LOG(GILD_TRUNCATED, "Map path too long"); return false;}
	return true;
}

// x, y, w, h in tiles: non-empty and entirely inside the map.
static bool rect_inside(const struct Map *m, u16 x, u16 y, u16 w, u16 h){
	return w > 0 && h > 0 && (u32)x + w <= m->w && (u32)y + h <= m->h;
}

// Checks everything that isn't about the file itself, so a map that would fail to
// load is also refused when saving. what: "write" or the path, for the log.
static bool validate(const struct Map *m, const char *what){
	if(m->w == 0 || m->h == 0 || (u32)m->w * m->h > MAP_MAX_TILES){
		GILD_LOG(GILD_INVALID_FORMAT, "%s: map is %ux%u, must be at least 1x1 and at most %u tiles",
		         what, (u32)m->w, (u32)m->h, MAP_MAX_TILES);
		return false;
	}
	if(m->character_count > MAP_MAX_CHARACTERS){
		GILD_LOG(GILD_INVALID_FORMAT, "%s: %u characters, the limit is %u", what, m->character_count, MAP_MAX_CHARACTERS);
		return false;
	}
	if((m->character_count && !m->characters) || (m->ceiling_count && !m->ceilings) ||
	   (m->transition_count && !m->transitions)){
		GILD_LOG(GILD_IS_NULL, "%s: a list has a count but no array", what);
		return false;
	}
	// Two entries with one uid would load the same character twice.
	for(u32 i = 0; i < m->character_count; i++){
		for(u32 j = i + 1; j < m->character_count; j++){
			if(m->characters[i].uid == m->characters[j].uid){
				GILD_LOG(GILD_DUPLICATE, "%s: character uid %u is on the map twice", what, m->characters[i].uid);
				return false;
			}
		}
	}
	for(u32 i = 0; i < m->ceiling_count; i++){
		const struct MapCeiling *c = &m->ceilings[i];
		if(!rect_inside(m, c->x, c->y, c->w, c->h)){
			GILD_LOG(GILD_OUT_OF_BOUNDS, "%s: ceiling %u (%u,%u %ux%u) isn't inside the map",
			         what, i, (u32)c->x, (u32)c->y, (u32)c->w, (u32)c->h);
			return false;
		}
	}
	for(u32 i = 0; i < m->transition_count; i++){
		const struct MapTransition *t = &m->transitions[i];
		if(!rect_inside(m, t->x, t->y, t->w, t->h)){
			GILD_LOG(GILD_OUT_OF_BOUNDS, "%s: transition %u (%u,%u %ux%u) isn't inside the map",
			         what, i, (u32)t->x, (u32)t->y, (u32)t->w, (u32)t->h);
			return false;
		}
		// target_pos is in the target map, which isn't loaded here: check it when the
		// transition is used.
	}
	return true;
}

// Reading: fills *t (zeroed by the caller), allocating as it goes.
// Writing: reads from *t; floors, props or flags may be NULL and are written as zeros.
static void io_map(struct Gild_IO *io, struct Map *t, const char *path){
	bool w = io->writing;
	gild_io_u32(io, &t->map_id);
	gild_io_u16(io, &t->w);
	gild_io_u16(io, &t->h);
	if(!w && io->ok && (t->w == 0 || t->h == 0 || (u32)t->w * t->h > MAP_MAX_TILES)){
		GILD_LOG(GILD_CORRUPT, "%s claims a %ux%u map", path, (u32)t->w, (u32)t->h);
		io->ok = false;
	}
	if(!io->ok){return;}

	u32 tiles = (u32)t->w * t->h;
	if(!w){
		t->floors = GILD_XCALLOC(tiles, sizeof(u32));
		t->props  = GILD_XCALLOC(tiles, sizeof(u32));
		t->flags  = GILD_XCALLOC(tiles, sizeof(u8));
	}
	for(u32 i = 0; io->ok && i < tiles; i++){
		u32 v = t->floors ? t->floors[i] : 0;
		gild_io_u32(io, &v);
		if(!w){t->floors[i] = v;}
	}
	for(u32 i = 0; io->ok && i < tiles; i++){
		u32 v = t->props ? t->props[i] : 0;
		gild_io_u32(io, &v);
		if(!w){t->props[i] = v;}
	}
	for(u32 i = 0; io->ok && i < tiles; i++){
		u8 v = t->flags ? (u8)(t->flags[i] & (u8)~MAP_FLAGS_RUNTIME) : 0;
		gild_io_u8(io, &v);
		if(!w){t->flags[i] = (u8)(v & (u8)~MAP_FLAGS_RUNTIME);} // an old file could still have it set
	}

	// Characters
	gild_io_u32(io, &t->character_count);
	if(!w && io->ok){
		if(t->character_count > MAP_MAX_CHARACTERS){
			GILD_LOG(GILD_CORRUPT, "%s claims %u characters", path, t->character_count);
			io->ok = false;
		} else if(t->character_count){
			t->characters = GILD_XCALLOC(t->character_count, sizeof(struct MapCharacter));
		}
	}
	for(u32 i = 0; io->ok && i < t->character_count; i++){
		gild_io_u32(io, &t->characters[i].uid);
		gild_io_u32(io, &t->characters[i].save_index);
	}

	// Ceilings
	gild_io_u16(io, &t->ceiling_count);
	if(!w && io->ok && t->ceiling_count){
		t->ceilings = GILD_XCALLOC(t->ceiling_count, sizeof(struct MapCeiling));
	}
	for(u32 i = 0; io->ok && i < t->ceiling_count; i++){
		struct MapCeiling *c = &t->ceilings[i];
		gild_io_u16(io, &c->x);
		gild_io_u16(io, &c->y);
		gild_io_u16(io, &c->w);
		gild_io_u16(io, &c->h);
		gild_io_u8(io, &c->sprite);
	}

	// Transitions
	gild_io_u16(io, &t->transition_count);
	if(!w && io->ok && t->transition_count){
		t->transitions = GILD_XCALLOC(t->transition_count, sizeof(struct MapTransition));
	}
	for(u32 i = 0; io->ok && i < t->transition_count; i++){
		struct MapTransition *tr = &t->transitions[i];
		gild_io_u16(io, &tr->x);
		gild_io_u16(io, &tr->y);
		gild_io_u16(io, &tr->w);
		gild_io_u16(io, &tr->h);
		gild_io_u32(io, &tr->target_id);
		gild_io_u32(io, &tr->target_pos.x);
		gild_io_u32(io, &tr->target_pos.y);
	}
}

// rename replaces the old file on POSIX but fails on Windows if it exists.
static bool replace_file(const char *from, const char *to){
	if(rename(from, to) == 0){return true;}
	remove(to);
	if(rename(from, to) == 0){return true;}
	GILD_LOG(GILD_WRITE, "Can't move %s to %s", from, to);
	remove(from);
	return false;
}

bool serialize_map_path(struct Map *m, const char *path, bool write){
	if(!m || !path){GILD_LOG(GILD_IS_NULL, "Passed NULL map or path"); return false;}

	char tmp_path[600];
	if(write){
		if(!validate(m, path)){return false;}
		int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
		if(n < 0 || (size_t)n >= sizeof(tmp_path)){GILD_LOG(GILD_TRUNCATED, "Map path too long"); return false;}
	}

	const char *open_path = write ? tmp_path : path;
	FILE *f = gild_fopen(open_path, write ? "wb" : "rb");
	if(!f){GILD_LOG(GILD_NO_FILE, "Can't open %s", open_path); return false;}

	struct Map tmp = {0};
	struct Map *t = write ? m : &tmp;

	struct Gild_IO io = gild_io_begin(f, write);
	u32 version = MAP_VERSION;
	gild_io_header(&io, MAP_MAGIC, &version, NULL);
	if(io.ok && version != MAP_VERSION){ // later: read older versions here instead of failing
		GILD_LOG(GILD_VERSION_MISMATCH, "%s is version %u, expected %u", path, version, MAP_VERSION);
		io.ok = false;
	}
	io_map(&io, t, path);

	// Reading: the file must end here. Extra bytes mean it isn't what we think it is.
	if(!write && io.ok && fgetc(f) != EOF){
		GILD_LOG(GILD_CORRUPT, "%s has data after the end of the map", path);
		io.ok = false;
	}

	bool ok = gild_io_end(&io);
	if(fclose(f) != 0){ok = false;}

	if(write){
		if(!ok){remove(tmp_path); return false;}
		return replace_file(tmp_path, path);
	}

	if(ok && !validate(&tmp, path)){ok = false;}
	if(!ok){free_map(&tmp); return false;}
	free_map(m);
	*m = tmp;
	return true;
}

bool serialize_map(struct Map *m, bool write){
	if(!m){GILD_LOG(GILD_IS_NULL, "Passed NULL map"); return false;}
	char path[512];
	if(!map_path(m->map_id, path, sizeof(path))){return false;}

	u32 want = m->map_id;
	if(write){return serialize_map_path(m, path, true);}

	// Read into a copy first so the id check can still leave *m untouched.
	struct Map loaded = {0};
	if(!serialize_map_path(&loaded, path, false)){return false;}
	if(loaded.map_id != want){
		GILD_LOG(GILD_CORRUPT, "%s holds map %u", path, loaded.map_id);
		free_map(&loaded);
		return false;
	}
	free_map(m);
	*m = loaded;
	return true;
}
