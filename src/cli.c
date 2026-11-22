#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "stats.h"
#include <string.h>
#include <strings.h>
#include "inventory.h"
#include "entity.h"
#include "util/gild_basic.h"
#include "cli.h"
#include "util/gild_log.h"
#include "util/gild_io.h"
#define MAX_LINE 1024
#define MAX_ARGS 64

struct EntityPrototype prototype;
char *name;
// Helper to case-insensitively match a field name like "strength" or "ST_STRENGTH"
static bool field_matches(const char *input, const char *enum_name, const char *prefix) {
	size_t plen = strlen(prefix);
	const char *bare = input;
	if (strncasecmp(input, prefix, plen) == 0) {
		bare = input + plen;
	}
	if (strcasecmp(input, enum_name) == 0 || strcasecmp(bare, enum_name) == 0) {
		return true;
	}
	return false;
}

static bool parse_s16(const char *str, s16 *out) {
	if (!str || *str == '\0') return false;
	errno = 0;
	char *end = NULL;
	long val = strtol(str, &end, 10);
	if (errno != 0 || end == str || *end != '\0') return false;
	if (val < -32768 || val > 32767) return false;
	*out = (s16)val;
	return true;
}

static bool parse_u32(const char *str, u32 *out) {
	if (!str || *str == '\0' || str[0] == '-') return false;
	errno = 0;
	char *end = NULL;
	unsigned long val = strtoul(str, &end, 10);
	if (errno != 0 || end == str || *end != '\0' || val > 0xFFFFFFFFUL) return false;
	*out = (u32)val;
	return true;
}

static void reset_prototype(void) {
	memset(&prototype, 0, sizeof(prototype));
	free(name);
	name = NULL;
	prototype.type = CIVILIAN_ENTITY;
	prototype.starting_level = 1;
}
static bool entity_type_matches(const char *value, const char *base_name) {
	char full_enum[128];
	int n = snprintf(full_enum, sizeof(full_enum), "%s_ENTITY", base_name);
	if (n < 0 || n >= (int)sizeof(full_enum)) {
		return false;
	}
	return strcasecmp(value, base_name) == 0 || strcasecmp(value, full_enum) == 0;
}
static void set_prototype_field(const char *field, const char *value) {
	if (strcasecmp(field, "name") == 0) {
		char *copy = strdup(value);
		if (!copy) {
			GILD_LOG(GILD_INFO, "Out of memory setting name");
			return;
		}
		free(name);
		name = copy;
		GILD_LOG(GILD_INFO, "Set name = %s", name);
		return;
	}
	if (strcasecmp(field, "type") == 0) {
		#define X(tname) \
			if (entity_type_matches(value, #tname)) { \
				prototype.type = tname##_ENTITY; \
				GILD_LOG(GILD_INFO, "Set type = %s", #tname "_ENTITY"); \
				return; \
			}
		ENTITY_TYPE_LIST
		#undef X

		GILD_LOG(GILD_INFO, "Unknown entity type '%s'", value);
		return;
	}

	if (strcasecmp(field, "level") == 0 || strcasecmp(field, "starting_level") == 0) {
		u32 lvl;
		if (!parse_u32(value, &lvl)) {
			GILD_LOG(GILD_INFO, "Invalid level value '%s'", value);
			return;
		}
		prototype.starting_level = lvl;
		GILD_LOG(GILD_INFO, "Set starting_level = %u", prototype.starting_level);
		return;
	}

	if (strcasecmp(field, "dialogue") == 0 || strcasecmp(field, "dialogue_path") == 0) {
		// Note: if you allocate dialogue_path dynamically, track/free it in reset_prototype()
		prototype.dialogue_path = strdup(value);
		GILD_LOG(GILD_INFO, "Set dialogue_path = %s", prototype.dialogue_path);
		return;
	}

	// Match base stats from STATS_LIST (e.g., "strength", "ST_STRENGTH", "agility")
	#define X(stat_name) \
		if (field_matches(field, #stat_name, "ST_")) { \
			s16 v; \
			if (!parse_s16(value, &v)) { \
				GILD_LOG(GILD_INFO, "Invalid integer for stat '%s': %s", field, value); \
				return; \
			} \
			prototype.stats.base_stats[ST_##stat_name] = v; \
			GILD_LOG(GILD_INFO, "Set base stat %s = %d", #stat_name, v); \
			return; \
		}
	STATS_LIST
	#undef X

	// Match skill mods from SKILLS_LIST (e.g., "guns", "SK_GUNS", "sneak")
	#define X(skill_name) \
		if (field_matches(field, #skill_name, "SK_")) { \
			s16 v; \
			if (!parse_s16(value, &v)) { \
				GILD_LOG(GILD_INFO, "Invalid integer for skill '%s': %s", field, value); \
				return; \
			} \
			prototype.stats.skill_mod[SK_##skill_name] = v; \
			GILD_LOG(GILD_INFO, "Set skill mod %s = %d", #skill_name, v); \
			return; \
		}
	SKILLS_LIST
	#undef X

	GILD_LOG(GILD_INFO, "Unknown prototype field '%s'", field);
}

#define PROTO_MAGIC   "GILDPRTO"
#define PROTO_VERSION 1u

static bool write_string_field(FILE *f, const char *str) {
	u32 len = str ? (u32)strlen(str) : 0u;
	if (!gild_write_u32(f, len)) {
		return false;
	}
	if (len > 0 && !gild_write_bytes(f, str, len)) {
		return false;
	}
	return true;
}

static bool write_item_stack(FILE *f, const struct ItemStack *stack) {
	return gild_write_u32(f, stack->id)
	    && gild_write_u32(f, stack->count)
	    && gild_write_u8(f, stack->active ? 1u : 0u);
}

static void write_prototype_file(const char *base_name) {
	if (!base_name || base_name[0] == '\0') {
		GILD_LOG(GILD_INVALID_ARG, "Invalid prototype output name");
		return;
	}

	char filename[512];
	int written = snprintf(filename, sizeof(filename), "%s.proto", base_name);
	if (written < 0 || written >= (int)sizeof(filename)) {
		GILD_LOG(GILD_INVALID_FORMAT, "Prototype path too long: %s.proto", base_name);
		return;
	}

	if (!gild_parent_dir_exists(filename)) {
		GILD_LOG(GILD_NO_FILE, "Parent directory does not exist for '%s'", filename);
		return;
	}

	FILE *fp = gild_fopen(filename, "wb");
	if (!fp) {
		GILD_LOG(GILD_NO_FILE, "Failed to open '%s' for writing", filename);
		return;
	}

	bool ok = gild_write_header(fp, PROTO_MAGIC, PROTO_VERSION)
	       && write_string_field(fp, name)
	       && gild_write_u32(fp, (u32)prototype.type)
	       && gild_write_u32(fp, prototype.starting_level)
	       && gild_write_u8(fp, prototype.flags)
	       && write_string_field(fp, prototype.dialogue_path)
	       && gild_write_u32(fp, prototype.collision.x)
	       && gild_write_u32(fp, prototype.collision.y)
	       && gild_write_u32(fp, prototype.collision.w)
	       && gild_write_u32(fp, prototype.collision.h);

	for (int i = 0; ok && i < ST_STAT_COUNT; i++) {
		ok = gild_write_s16(fp, prototype.stats.base_stats[i]);
	}

	for (int i = 0; ok && i < SK_SKILL_COUNT; i++) {
		ok = gild_write_s16(fp, prototype.stats.skill_mod[i]);
	}

	for (int i = 0; ok && i < HOTBAR_SIZE; i++) {
		ok = write_item_stack(fp, &prototype.starting_inventory.hotbar_items[i]);
	}

	for (int i = 0; ok && i < INVENTORY_SIZE; i++) {
		ok = write_item_stack(fp, &prototype.starting_inventory.items[i]);
	}

	if (fclose(fp) != 0) {
		ok = false;
	}

	if (!ok) {
		GILD_LOG(GILD_WRITE, "Failed while writing prototype to '%s'", filename);
		return;
	}

	GILD_LOG(GILD_INFO, "Wrote prototype to '%s'", filename);
}
static void handle_command(int argc, char *argv[]) {
	if (argc <= 0 || !argv[0]) return;

	if (strcmp(argv[0], "help") == 0) {
		GILD_LOG(GILD_INFO,
			"Available Commands:\n"
			"  help: show this message\n"
			"  exit/quit: exit program\n"
			"  set FIELD VALUE: set a field on prototype (name, type, level, dialogue, stats, skills)\n"
			"  write NAME: write prototype to NAME.proto\n"
			"  reset: clears the prototype"
		);
	} else if (strcmp(argv[0], "set") == 0) {
		if (argc != 3) {
			GILD_LOG(GILD_INFO, "Invalid arguments for set, requires FIELD VALUE");
			return;
		}
		set_prototype_field(argv[1], argv[2]);
	} else if (strcmp(argv[0], "write") == 0) {
		if (argc != 2) {
			GILD_LOG(GILD_INFO, "Invalid arguments for write, requires NAME");
			return;
		}
		write_prototype_file(argv[1]);
	} else if (strcmp(argv[0], "reset") == 0) {
		reset_prototype();
		GILD_LOG(GILD_INFO, "Prototype reset");
	} else {
		GILD_LOG(GILD_INFO, "Unknown command: %s. Type help for help", argv[0]);
	}
}

bool run_cli(){
	GILD_LOG(GILD_INFO, "Entering CLI mode. Type 'help' or 'exit' to quit");
	
	char line[MAX_LINE];

	while(1){
		printf("> ");
		fflush(stdout);
		
		if(fgets(line, sizeof(line), stdin) == NULL){printf("\n");return false;}

		line[strcspn(line, "\r\n")] = '\0';
		
		char *argv[MAX_ARGS];
		int argc = 0;
		char *token = strtok(line, " \t");

		while(token != NULL && argc < MAX_ARGS - 1){
			argv[argc++] = token;
			token = strtok(NULL, " \t");
		}
		argv[argc] = NULL;

		if(argc == 0){continue;}

		if(strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0){return false;}
		handle_command(argc, argv);
	}
}
