#include <stdlib.h>
#include <string.h>
#include "util/gild_basic.h"
#include "util/gild_io.h"
#include "util/gild_log.h"
#include "util/gild_mem.h"
#include "save.h"

// File formats. Every file starts with the gild header (8 magic bytes, u64 timestamp,
// u32 version). Everything goes through gild_io, so integers are little-endian and
// nothing depends on struct layout or padding.
//
//   str          u32 byte length, then the bytes. Length 0 means NULL.
//   item stack   u32 id, u32 count, u8 active
//   array        u16 element count, then the elements. On read the count must match
//                this build, so a changed STATS_LIST / SKILLS_LIST / HOTBAR_SIZE /
//                INVENTORY_SIZE is reported instead of silently misread.
//
//   Entity ("ENTSAVE_")       s16 array permanent_bstat_upgrade, s16 array permanent_skill_upgrade,
//                             item stack array hotbar_items, item stack array items,
//                             str prototype_path, u32 tpos.x, u32 tpos.y, u32 actual_level, u8 actual_flags
//   Map ("MAPFILE_")          u16 w, u16 h, u8 floors[w*h], u8 flags[w*h],
//                             u16 ceiling count, then per ceiling: u16 x, u16 y, u16 w, u16 h, u8 sprite
//                             u16 transition count, then per transition:
//                             u16 x, u16 y, u16 w, u16 h, str path, u32 target.x, u32 target.y
//   Map entities ("MAPENTS_") u16 count, then per entity:
//                             str prototype, u8 type, u32 uid, u32 pos.x, u32 pos.y, u8 sprite, u8 should_collide
//   Flags ("FLAGSAVE")        u32 count, then per flag: str name, u8 value
//                             The hash is not stored; flag_set recomputes it on load, so changing
//                             the hash function doesn't break old saves.
//
// Bump the matching version below whenever one of these layouts changes.

#define ENTITY_SAVE_MAGIC   "ENTSAVE_"
#define ENTITY_SAVE_VERSION 1u
#define ENTITY_SAVE_EXT     "ent"

#define MAP_SAVE_MAGIC      "MAPFILE_"
#define MAP_SAVE_VERSION    2u

#define MAP_ENTITY_MAGIC    "MAPENTS_"
#define MAP_ENTITY_VERSION  1u

#define FLAG_SAVE_MAGIC     "FLAGSAVE"
#define FLAG_SAVE_VERSION   1u

// Longest string accepted from disk, so a corrupt length can't cause a huge allocation.
// Writes refuse longer strings too, so we never produce a file we can't read back.
#define SAVE_STR_MAX 4096u

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool str_fits(const char *s){
	return !s || strlen(s) <= SAVE_STR_MAX;
}

// Reads an array length and checks it against the size compiled into this build.
static bool read_array_len(FILE *f, u16 expected, const char *what){
	u16 got;
	if(!gild_read_u16(f, &got)){return false;}
	if(got != expected){
		GILD_LOG(GILD_VERSION_MISMATCH, "%s: file has %u entries, this build expects %u",
		         what, (u32)got, (u32)expected);
		return false;
	}
	return true;
}

static bool write_s16_array(FILE *f, const s16 *a, u16 n){
	if(!gild_write_u16(f, n)){return false;}
	for(u16 i = 0; i < n; i++){
		if(!gild_write_s16(f, a[i])){return false;}
	}
	return true;
}

static bool read_s16_array(FILE *f, s16 *a, u16 n, const char *what){
	if(!read_array_len(f, n, what)){return false;}
	for(u16 i = 0; i < n; i++){
		if(!gild_read_s16(f, &a[i])){return false;}
	}
	return true;
}

static bool write_item_stacks(FILE *f, const struct ItemStack *a, u16 n){
	if(!gild_write_u16(f, n)){return false;}
	for(u16 i = 0; i < n; i++){
		if(!(gild_write_u32(f, a[i].id) &&
		     gild_write_u32(f, a[i].count) &&
		     gild_write_u8(f, a[i].active ? 1 : 0))){return false;}
	}
	return true;
}

static bool read_item_stacks(FILE *f, struct ItemStack *a, u16 n, const char *what){
	if(!read_array_len(f, n, what)){return false;}
	for(u16 i = 0; i < n; i++){
		u8 active = 0;
		if(!(gild_read_u32(f, &a[i].id) &&
		     gild_read_u32(f, &a[i].count) &&
		     gild_read_u8(f, &active))){return false;}
		a[i].active = active != 0;
	}
	return true;
}

static void free_entity_array(struct MapEntity *ents, u16 count){
	if(!ents){return;}
	for(u16 i = 0; i < count; i++){free((void *)ents[i].prototype);}
	free(ents);
}

// Opens path for writing after checking that its directory exists.
static FILE *open_for_write(const char *path){
	if(!gild_parent_dir_exists(path)){
		GILD_LOG(GILD_NO_FILE, "Directory for %s does not exist", path);
		return NULL;
	}
	FILE *f = gild_fopen(path, "wb");
	if(!f){GILD_LOG(GILD_OPEN, "Failed to open %s for writing", path);}
	return f;
}

// fclose can fail on the final flush, which means the data never reached the disk.
static bool finish_write(FILE *f, bool ok, const char *path){
	if(fclose(f) != 0){ok = false;}
	if(!ok){GILD_LOG(GILD_WRITE, "Failed to write %s", path);}
	return ok;
}

static FILE *open_for_read(const char *path){
	FILE *f = gild_fopen(path, "rb");
	if(!f){GILD_LOG(GILD_NO_FILE, "Failed to open %s", path);}
	return f;
}

static bool finish_read(FILE *f, bool ok, const char *path){
	fclose(f);
	if(!ok){GILD_LOG(GILD_READ, "Failed to read %s", path);}
	return ok;
}

// ---------------------------------------------------------------------------
// EntitySaveable
// ---------------------------------------------------------------------------

static char *entity_path(u32 uid){
	return gild_format_path(ENTITY_SAVE_DIR, ENTITY_SAVE_EXT, gild_safe_cast(s32, uid));
}

bool write_entity(const struct EntitySaveable *s, u32 uid){
	if(!s){GILD_LOG(GILD_IS_NULL, "Passed NULL entity"); return false;}
	if(!str_fits(s->prototype_path)){
		GILD_LOG(GILD_INVALID_ARG, "Entity %u prototype path is over %u bytes", uid, SAVE_STR_MAX);
		return false;
	}

	char *path = entity_path(uid);
	if(!path){return false;}
	FILE *f = open_for_write(path);
	if(!f){free(path); return false;}

	bool ok = gild_write_header(f, ENTITY_SAVE_MAGIC, ENTITY_SAVE_VERSION) &&
	          write_s16_array(f, s->actual_stats.permanent_bstat_upgrade, ST_STAT_COUNT) &&
	          write_s16_array(f, s->actual_stats.permanent_skill_upgrade, SK_SKILL_COUNT) &&
	          write_item_stacks(f, s->actual_inventory.hotbar_items, HOTBAR_SIZE) &&
	          write_item_stacks(f, s->actual_inventory.items, INVENTORY_SIZE) &&
	          gild_write_str(f, s->prototype_path) &&
	          gild_write_u32(f, s->tpos.x) &&
	          gild_write_u32(f, s->tpos.y) &&
	          gild_write_u32(f, s->actual_level) &&
	          gild_write_u8(f, s->actual_flags);
	ok = finish_write(f, ok, path);
	free(path);
	return ok;
}

bool read_entity(struct EntitySaveable *out, u32 uid){
	if(!out){GILD_LOG(GILD_IS_NULL, "Passed NULL entity"); return false;}

	char *path = entity_path(uid);
	if(!path){return false;}
	FILE *f = open_for_read(path);
	if(!f){free(path); return false;}

	// Read into a temporary so a failed read never leaves *out half-written.
	struct EntitySaveable tmp = {0};
	char *proto = NULL;
	bool ok = gild_read_header(f, ENTITY_SAVE_MAGIC, ENTITY_SAVE_VERSION) &&
	          read_s16_array(f, tmp.actual_stats.permanent_bstat_upgrade, ST_STAT_COUNT, "Base stats") &&
	          read_s16_array(f, tmp.actual_stats.permanent_skill_upgrade, SK_SKILL_COUNT, "Skills") &&
	          read_item_stacks(f, tmp.actual_inventory.hotbar_items, HOTBAR_SIZE, "Hotbar") &&
	          read_item_stacks(f, tmp.actual_inventory.items, INVENTORY_SIZE, "Inventory") &&
	          gild_read_str(f, &proto, SAVE_STR_MAX) &&
	          gild_read_u32(f, &tmp.tpos.x) &&
	          gild_read_u32(f, &tmp.tpos.y) &&
	          gild_read_u32(f, &tmp.actual_level) &&
	          gild_read_u8(f, &tmp.actual_flags);
	ok = finish_read(f, ok, path);
	free(path);
	if(!ok){
		free(proto);
		return false;
	}
	tmp.prototype_path = proto;
	*out = tmp;
	return true;
}

bool entity_save_exists(u32 uid){
	char *path = entity_path(uid);
	if(!path){return false;}
	FILE *f = gild_fopen(path, "rb");
	free(path);
	if(!f){return false;}
	fclose(f);
	return true;
}

void free_entity_saveable(struct EntitySaveable *s){
	if(!s){return;}
	free((void *)s->prototype_path);
	s->prototype_path = NULL;
}

// ---------------------------------------------------------------------------
// Map
// ---------------------------------------------------------------------------

static bool map_size_ok(u16 w, u16 h){
	return w > 0 && h > 0 && (u32)w * (u32)h <= MAP_MAX_TILES;
}
static bool rect_ok(u16 x, u16 y, u16 w, u16 h, u16 map_w, u16 map_h){
	return w > 0 && h > 0 && (u32)x + w <= map_w && (u32)y + h <= map_h;
}
static bool ceiling_ok(const struct MapCeiling *c, u16 map_w, u16 map_h){
	return rect_ok(c->x, c->y, c->w, c->h, map_w, map_h);
}
static bool transition_ok(const struct MapTransition *t, u16 map_w, u16 map_h){
	return rect_ok(t->x, t->y, t->w, t->h, map_w, map_h) && t->path && str_fits(t->path);
}
static void free_transition_array(struct MapTransition *ts, u16 count){
	if(!ts){return;}
	for(u16 i = 0; i < count; i++){free((void *)ts[i].path);}
	free(ts);
}

bool write_map(const struct Map *m, const char *path){
	if(!m || !path){GILD_LOG(GILD_IS_NULL, "Passed NULL map or path"); return false;}
	if(!map_size_ok(m->w, m->h)){
		GILD_LOG(GILD_INVALID_STATE, "Map size %ux%u is empty or over %u tiles", (u32)m->w, (u32)m->h, MAP_MAX_TILES);
		return false;
	}
	if(!m->floors || !m->flags){GILD_LOG(GILD_INVALID_STATE, "Map has no floor or flag array"); return false;}
	if(m->ceiling_count > 0 && !m->ceilings){
		GILD_LOG(GILD_INVALID_STATE, "Map has %u ceilings but no ceiling array", (u32)m->ceiling_count);
		return false;
	}
	for(u16 i = 0; i < m->ceiling_count; i++){
		if(!ceiling_ok(&m->ceilings[i], m->w, m->h)){
			GILD_LOG(GILD_INVALID_STATE, "Ceiling %u is empty or outside the map", (u32)i);
			return false;
		}
	}
	if(m->transition_count > 0 && !m->transitions){
		GILD_LOG(GILD_INVALID_STATE, "Map has %u transitions but no transition array", (u32)m->transition_count);
		return false;
	}
	for(u16 i = 0; i < m->transition_count; i++){
		if(!transition_ok(&m->transitions[i], m->w, m->h)){
			GILD_LOG(GILD_INVALID_STATE, "Transition %u is empty, outside the map, or has no path", (u32)i);
			return false;
		}
	}

	FILE *f = open_for_write(path);
	if(!f){return false;}

	size_t n = (size_t)m->w * m->h;
	bool ok = gild_write_header(f, MAP_SAVE_MAGIC, MAP_SAVE_VERSION) &&
	          gild_write_u16(f, m->w) &&
	          gild_write_u16(f, m->h) &&
	          gild_write_bytes(f, m->floors, n) &&
	          gild_write_bytes(f, m->flags, n) &&
	          gild_write_u16(f, m->ceiling_count);
	for(u16 i = 0; ok && i < m->ceiling_count; i++){
		const struct MapCeiling *c = &m->ceilings[i];
		ok = gild_write_u16(f, c->x) &&
		     gild_write_u16(f, c->y) &&
		     gild_write_u16(f, c->w) &&
		     gild_write_u16(f, c->h) &&
		     gild_write_u8(f, c->sprite);
	}
	ok = ok && gild_write_u16(f, m->transition_count);
	for(u16 i = 0; ok && i < m->transition_count; i++){
		const struct MapTransition *t = &m->transitions[i];
		ok = gild_write_u16(f, t->x) &&
		     gild_write_u16(f, t->y) &&
		     gild_write_u16(f, t->w) &&
		     gild_write_u16(f, t->h) &&
		     gild_write_str(f, t->path) &&
		     gild_write_u32(f, t->target.x) &&
		     gild_write_u32(f, t->target.y);
	}
	return finish_write(f, ok, path);
}

bool read_map(struct Map *out, const char *path){
	if(!out || !path){GILD_LOG(GILD_IS_NULL, "Passed NULL map or path"); return false;}
	FILE *f = open_for_read(path);
	if(!f){return false;}

	u16 w = 0, h = 0, ccount = 0, tcount = 0;
	u8 *floors = NULL, *flags = NULL;
	struct MapCeiling *ceilings = NULL;
	struct MapTransition *transitions = NULL;

	bool ok = gild_read_header(f, MAP_SAVE_MAGIC, MAP_SAVE_VERSION) &&
	          gild_read_u16(f, &w) &&
	          gild_read_u16(f, &h);
	if(ok && !map_size_ok(w, h)){
		GILD_LOG(GILD_CORRUPT, "Map size %ux%u is empty or over %u tiles", (u32)w, (u32)h, MAP_MAX_TILES);
		ok = false;
	}
	size_t n = (size_t)w * h;
	if(ok){
		floors = GILD_XMALLOC(n);
		flags = GILD_XMALLOC(n);
		ok = gild_read_bytes(f, floors, n) &&
		     gild_read_bytes(f, flags, n) &&
		     gild_read_u16(f, &ccount);
	}
	if(ok && ccount > 0){ceilings = GILD_XCALLOC(ccount, sizeof(struct MapCeiling));}
	for(u16 i = 0; ok && i < ccount; i++){
		struct MapCeiling *c = &ceilings[i];
		ok = gild_read_u16(f, &c->x) &&
		     gild_read_u16(f, &c->y) &&
		     gild_read_u16(f, &c->w) &&
		     gild_read_u16(f, &c->h) &&
		     gild_read_u8(f, &c->sprite);
		if(ok && !ceiling_ok(c, w, h)){
			GILD_LOG(GILD_CORRUPT, "Ceiling %u is empty or outside the map", (u32)i);
			ok = false;
		}
	}

	ok = ok && gild_read_u16(f, &tcount);
	// calloc so every path starts NULL and free_transition_array is safe at any point.
	if(ok && tcount > 0){transitions = GILD_XCALLOC(tcount, sizeof(struct MapTransition));}
	for(u16 i = 0; ok && i < tcount; i++){
		struct MapTransition *t = &transitions[i];
		char *tpath = NULL;
		ok = gild_read_u16(f, &t->x) &&
		     gild_read_u16(f, &t->y) &&
		     gild_read_u16(f, &t->w) &&
		     gild_read_u16(f, &t->h) &&
		     gild_read_str(f, &tpath, SAVE_STR_MAX) &&
		     gild_read_u32(f, &t->target.x) &&
		     gild_read_u32(f, &t->target.y);
		t->path = tpath; // the array owns it from here, so the cleanup below frees it
		if(ok && !transition_ok(t, w, h)){
			GILD_LOG(GILD_CORRUPT, "Transition %u is empty, outside the map, or has no path", (u32)i);
			ok = false;
		}
	}

	ok = finish_read(f, ok, path);
	if(!ok){
		free(floors);
		free(flags);
		free(ceilings);
		free_transition_array(transitions, tcount);
		return false;
	}
	out->w = w;
	out->h = h;
	out->floors = floors;
	out->flags = flags;
	out->ceilings = ceilings;
	out->ceiling_count = ccount;
	out->transitions = transitions;
	out->transition_count = tcount;
	out->player_state = 0;
	out->player = 0;
	return true;
}

char *map_entity_path(const char *map_path){
	if(!map_path){GILD_LOG(GILD_IS_NULL, "Passed NULL map path"); return NULL;}
	// The extension is the last '.' after the last separator ("maps.d/town" has none).
	const char *sep = strrchr(map_path, '/');
	const char *back = strrchr(map_path, '\\');
	if(back && (!sep || back > sep)){sep = back;}
	const char *dot = strrchr(map_path, '.');
	size_t base = (dot && (!sep || dot > sep)) ? (size_t)(dot - map_path) : strlen(map_path);

	const char *ext = ".ments";
	size_t len = gild_safe_add(base, strlen(ext));
	char *r = GILD_XMALLOC(gild_safe_add(len, (size_t)1));
	memcpy(r, map_path, base);
	memcpy(r + base, ext, strlen(ext) + 1);
	return r;
}

void free_map(struct Map *m){
	if(!m){return;}
	free(m->floors);
	free(m->flags);
	free(m->ceilings);
	free_transition_array(m->transitions, m->transition_count);
	m->floors = NULL;
	m->flags = NULL;
	m->ceilings = NULL;
	m->ceiling_count = 0;
	m->transitions = NULL;
	m->transition_count = 0;
	m->w = 0;
	m->h = 0;
	free_map_entities(m);
}

// ---------------------------------------------------------------------------
// Map entities
// ---------------------------------------------------------------------------

bool write_map_entities(const struct Map *m, const char *path){
	if(!m || !path){GILD_LOG(GILD_IS_NULL, "Passed NULL map or path"); return false;}
	if(m->entity_count > 0 && !m->entities){
		GILD_LOG(GILD_INVALID_STATE, "Map has %u entities but no entity array", (u32)m->entity_count);
		return false;
	}
	for(u16 i = 0; i < m->entity_count; i++){
		const struct MapEntity *e = &m->entities[i];
		if((u32)e->type > (u32)PROP){
			GILD_LOG(GILD_INVALID_STATE, "Map entity %u has invalid type %u", (u32)i, (u32)e->type);
			return false;
		}
		if(!str_fits(e->prototype)){
			GILD_LOG(GILD_INVALID_ARG, "Map entity %u prototype path is over %u bytes", (u32)i, SAVE_STR_MAX);
			return false;
		}
	}

	FILE *f = open_for_write(path);
	if(!f){return false;}

	bool ok = gild_write_header(f, MAP_ENTITY_MAGIC, MAP_ENTITY_VERSION) &&
	          gild_write_u16(f, m->entity_count);
	for(u16 i = 0; ok && i < m->entity_count; i++){
		const struct MapEntity *e = &m->entities[i];
		ok = gild_write_str(f, e->prototype) &&
		     gild_write_u8(f, (u8)e->type) &&
		     gild_write_u32(f, e->uid) &&
		     gild_write_u32(f, e->pos.x) &&
		     gild_write_u32(f, e->pos.y) &&
		     gild_write_u8(f, e->sprite) &&
		     gild_write_u8(f, e->should_collide);
	}
	return finish_write(f, ok, path);
}

bool read_map_entities(struct Map *m, const char *path){
	if(!m || !path){GILD_LOG(GILD_IS_NULL, "Passed NULL map or path"); return false;}
	FILE *f = open_for_read(path);
	if(!f){return false;}

	u16 count = 0;
	struct MapEntity *ents = NULL;
	bool ok = gild_read_header(f, MAP_ENTITY_MAGIC, MAP_ENTITY_VERSION) &&
	          gild_read_u16(f, &count);
	// calloc so every prototype starts NULL and free_entity_array is safe at any point.
	if(ok && count > 0){ents = GILD_XCALLOC(count, sizeof(struct MapEntity));}

	for(u16 i = 0; ok && i < count; i++){
		struct MapEntity *e = &ents[i];
		char *proto = NULL;
		u8 type = 0;
		ok = gild_read_str(f, &proto, SAVE_STR_MAX) &&
		     gild_read_u8(f, &type) &&
		     gild_read_u32(f, &e->uid) &&
		     gild_read_u32(f, &e->pos.x) &&
		     gild_read_u32(f, &e->pos.y) &&
		     gild_read_u8(f, &e->sprite) &&
		     gild_read_u8(f, &e->should_collide);
		e->prototype = proto; // the array owns it from here, so the cleanup below frees it
		if(ok && type > PROP){
			GILD_LOG(GILD_CORRUPT, "Map entity %u has invalid type %u", (u32)i, (u32)type);
			ok = false;
		}
		if(ok){e->type = (enum MapEntityType)type;}
	}

	ok = finish_read(f, ok, path);
	if(!ok){
		free_entity_array(ents, count);
		return false;
	}
	m->entities = ents;
	m->entity_count = count;
	return true;
}

void free_map_entities(struct Map *m){
	if(!m){return;}
	free_entity_array(m->entities, m->entity_count);
	m->entities = NULL;
	m->entity_count = 0;
}

// ---------------------------------------------------------------------------
// Flags
// ---------------------------------------------------------------------------

bool write_flags(const struct FlagManager *fm, const char *path){
	if(!fm || !path){GILD_LOG(GILD_IS_NULL, "Passed NULL flag manager or path"); return false;}
	if(fm->count > 0 && !fm->flags){
		GILD_LOG(GILD_INVALID_STATE, "Flag manager has %u flags but no flag array", fm->count);
		return false;
	}
	for(u32 i = 0; i < fm->count; i++){
		// An empty name would read back as NULL, and an unterminated one can't be strlen'd.
		const char *name = fm->flags[i].name;
		if(name[0] == '\0' || !memchr(name, '\0', MAX_FLAG_NAME)){
			GILD_LOG(GILD_INVALID_STATE, "Flag %u has an empty or unterminated name", i);
			return false;
		}
	}

	FILE *f = open_for_write(path);
	if(!f){return false;}

	bool ok = gild_write_header(f, FLAG_SAVE_MAGIC, FLAG_SAVE_VERSION) &&
	          gild_write_u32(f, fm->count);
	for(u32 i = 0; ok && i < fm->count; i++){
		ok = gild_write_str(f, fm->flags[i].name) &&
		     gild_write_u8(f, fm->flags[i].value);
	}
	return finish_write(f, ok, path);
}

bool read_flags(struct FlagManager *out, const char *path){
	if(!out || !path){GILD_LOG(GILD_IS_NULL, "Passed NULL flag manager or path"); return false;}
	FILE *f = open_for_read(path);
	if(!f){return false;}

	// Rebuild through flag_set so hashing and growth stay flag.c's business.
	struct FlagManager tmp = {0};
	u32 count = 0;
	bool ok = gild_read_header(f, FLAG_SAVE_MAGIC, FLAG_SAVE_VERSION) &&
	          gild_read_u32(f, &count);
	for(u32 i = 0; ok && i < count; i++){
		char *name = NULL;
		u8 value = 0;
		ok = gild_read_str(f, &name, MAX_FLAG_NAME - 1) &&
		     gild_read_u8(f, &value);
		if(ok && !name){
			GILD_LOG(GILD_CORRUPT, "Flag %u has an empty name", i);
			ok = false;
		}
		if(ok){flag_set(&tmp, name, value != 0);}
		free(name);
	}

	ok = finish_read(f, ok, path);
	if(!ok){
		flag_free(&tmp);
		return false;
	}
	*out = tmp;
	return true;
}
