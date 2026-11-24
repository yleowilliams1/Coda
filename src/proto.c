#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>
#include "util/gild_io.h"
#include "util/gild_mem.h"
#include "util/gild_log.h"
#include "util/gild_string.h"
#include "util/gild_math.h"
#include "util/gild_basic.h"
#include "character.h"

#define CHARACTER_PROTO_PATH "data/proto/characters/"
#define ITEM_PROTO_PATH "data/proto/items/"

#define CHARACTER_PROTO_COUNT 5
#define ITEM_PROTO_COUNT 5

#define PROTO_STAT_MIN 1
#define PROTO_STAT_MAX 20

static struct ItemPrototype *iproto = NULL;
static struct CharacterPrototype *cproto = NULL;

static void cproto_parse(const struct Gild_ConfigPack *pack, void *ptr);
static void iproto_parse(const struct Gild_ConfigPack *pack, void *ptr);
const struct ItemPrototype *get_item_proto(u32 id){
	if(!iproto || id >= ITEM_PROTO_COUNT || !iproto[id].name){return &iproto[0];}
	return &iproto[id];
}

const struct CharacterPrototype *get_character_proto(u32 id){
	if(!cproto || id >= CHARACTER_PROTO_COUNT){return &cproto[0];}
	return &cproto[id];
}
void load_item_proto(){
	if(iproto){
		for(int i = 0; i < ITEM_PROTO_COUNT; i++){free(iproto[i].name); free(iproto[i].description);}
		free(iproto);
		iproto = NULL;
	}
	iproto = GILD_XCALLOC(ITEM_PROTO_COUNT, sizeof(struct ItemPrototype));
	for(int i = 0; i < ITEM_PROTO_COUNT; i++){
		char *path = gild_format_path(ITEM_PROTO_PATH, "ini", i);
		if(!FileExists(path)){
			GILD_LOG(GILD_NO_FILE, "No file %s", path);
		} else {
			iproto[i].max_stack = 1;
			iproto[i].effective_skill = ITEM_NO_SKILL;

			gild_config(&iproto[i], path, iproto_parse);

			if(!iproto[i].name){GILD_LOG(GILD_PARSE, "%s: missing name", path);}
			if(iproto[i].use_health_mod_min > iproto[i].use_health_mod_max){
				GILD_LOG(GILD_PARSE, "%s: use_health_mod_min is above use_health_mod_max", path);
			}
		}
		free(path);
	}
}
void load_character_proto(){
	if(cproto){free(cproto); cproto = NULL;}
	cproto = GILD_XCALLOC(CHARACTER_PROTO_COUNT, sizeof(struct CharacterPrototype));
	for(int i = 0; i < CHARACTER_PROTO_COUNT; i++){
		char *path = gild_format_path(CHARACTER_PROTO_PATH, "ini", i);
		if(!FileExists(path)){
			GILD_LOG(GILD_NO_FILE, "No file %s", path);
		} else {
			cproto[i].sprite_w = 1;
			cproto[i].sprite_h = 1;

			gild_config(&cproto[i], path, cproto_parse);

			#define X(name) if(cproto[i].stat[ST_##name] == 0){GILD_LOG(GILD_PARSE, "%s: %s is missing or invalid", path, #name);}
			STAT_LIST
			#undef X
		}
		free(path);
	}
}

// Parse helpers. Each logs its own error, naming the key and the bad value.

// Case-insensitive, so "Strength", "strength" and "STRENGTH" all match.
static bool matches(const char *a, const char *b){
	while(*a && *b){
		if(tolower((unsigned char)*a) != tolower((unsigned char)*b)){return false;}
		a++;
		b++;
	}
	return *a == '\0' && *b == '\0';
}

static bool read_int(const struct Gild_ConfigPack *pack, long long lo, long long hi, long long *out){
	char *end;
	errno = 0;
	long long v = strtoll(pack->value, &end, 10);
	if(end == pack->value || *end != '\0' || errno == ERANGE || v < lo || v > hi){
		GILD_LOG(GILD_PARSE, "%s = '%s' should be a whole number from %lld to %lld", pack->key, pack->value, lo, hi);
		return false;
	}
	*out = v;
	return true;
}

// "x y w h": four whole numbers, w and h above 0.
static bool read_rect_s16(const struct Gild_ConfigPack *pack, gild_rec_s16 *out){
	const char *s = pack->value;
	gild_rec_s16 r = {0};
	bool good = true;
	for(int i = 0; good && i < 4; i++){
		char *end;
		errno = 0;
		long long v = strtoll(s, &end, 10);
		good = end != s && errno != ERANGE && v >= INT16_MIN && v <= INT16_MAX;
		r.e[i] = (s16)v;
		s = end;
	}
	while(*s == ' ' || *s == '\t'){s++;}
	if(!good || *s != '\0' || r.w <= 0 || r.h <= 0){
		GILD_LOG(GILD_PARSE, "%s = '%s' should be four whole numbers: x y w h (w and h above 0)", pack->key, pack->value);
		return false;
	}
	*out = r;
	return true;
}

// Skill names come from SKILLS_LIST, so "melee" is SK_MELEE. "none" means no skill.
static bool read_skill(const struct Gild_ConfigPack *pack, u8 *out){
	if(matches(pack->value, "none")){*out = ITEM_NO_SKILL; return true;}
	#define X(name) if(matches(pack->value, #name)){*out = ST_##name; return true;}
	SKILL_LIST
	#undef X
	GILD_LOG(GILD_PARSE, "%s = '%s' is not a skill", pack->key, pack->value);
	return false;
}

// Replaces the string rather than leaking it if the key appears twice.
static void read_str(char **dst, const char *value){
	free(*dst);
	*dst = gild_copy_alloc_str(value, strlen(value));
}

// ---------------------------------------------------------------------------
// Parsers: one file per prototype, keys are the struct field names.
// ---------------------------------------------------------------------------

// data/proto/characters/<id>.ini
//   strength = 6          (all eight stats, 1 to 20)
//   agility = 12
//   ...
//   collision = 0 0 32 48
//   sprite_index = 14
//   sprite_w = 4
//   sprite_h = 8
static void cproto_parse(const struct Gild_ConfigPack *pack, void *ptr){
	struct CharacterPrototype *p = ptr;
	const char *k = pack->key;
	long long v;

	// Stats: the key is the stat's name from STATS_LIST, e.g. "strength = 6".
	#define X(name) \
		if(matches(k, #name)){ \
			if(read_int(pack, PROTO_STAT_MIN, PROTO_STAT_MAX, &v)){p->stat[ST_##name] = (s8)v;} \
			return; \
		}
	STAT_LIST
	#undef X

	if(matches(k, "collision")){read_rect_s16(pack, &p->collision);}
	else if(matches(k, "sprite_index")){
		if(read_int(pack, 0, UINT32_MAX, &v)){p->sprite_index = (u32)v;}
	}
	else if(matches(k, "sprite_w")){
		if(read_int(pack, 1, UINT32_MAX, &v)){p->sprite_w = (u32)v;}
	}
	else if(matches(k, "sprite_h")){
		if(read_int(pack, 1, UINT32_MAX, &v)){p->sprite_h = (u32)v;}
	}
	else{
		GILD_LOG(GILD_IGNORED, "Unknown character key '%s' ignored", k); // usually a typo
	}
}

// data/proto/items/<id>.ini
//   name = Dagger
//   description = A shiny dagger. Please don't eat.
//   value = 5
//   effective_skill = melee      (a name from SKILLS_LIST, or none)
//   use_health_mod_min = -6
//   use_health_mod_max = -2
//   AC = 0
//   max_stack = 1
//   bonus = 0
static void iproto_parse(const struct Gild_ConfigPack *pack, void *ptr){
	struct ItemPrototype *p = ptr;
	const char *k = pack->key;
	long long v;

	if(matches(k, "name")){read_str(&p->name, pack->value);}
	else if(matches(k, "description")){read_str(&p->description, pack->value);}
	else if(matches(k, "value")){
		if(read_int(pack, 0, UINT16_MAX, &v)){p->value = (u16)v;}
	}
	else if(matches(k, "effective_skill")){read_skill(pack, &p->effective_skill);}
	else if(matches(k, "use_health_mod_min")){
		if(read_int(pack, INT16_MIN, INT16_MAX, &v)){p->use_health_mod_min = (s16)v;}
	}
	else if(matches(k, "use_health_mod_max")){
		if(read_int(pack, INT16_MIN, INT16_MAX, &v)){p->use_health_mod_max = (s16)v;}
	}
	else if(matches(k, "AC")){
		if(read_int(pack, INT8_MIN, INT8_MAX, &v)){p->AC = (s8)v;}
	}
	else if(matches(k, "max_stack")){
		if(read_int(pack, 1, INT8_MAX, &v)){p->max_stack = (s8)v;}
	}
	else if(matches(k, "bonus")){
		if(read_int(pack, INT8_MIN, INT8_MAX, &v)){p->bonus = (s8)v;}
	}
	else{
		GILD_LOG(GILD_IGNORED, "Unknown item key '%s' ignored", k); // usually a typo
	}
}
