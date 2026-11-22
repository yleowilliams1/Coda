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

// My vibe coded masterpiece

struct EntityPrototype prototype;
char *name;
static bool parse_bool_flag(const char *str, bool *out) {
	if (!str) return false;
	if (strcasecmp(str, "1") == 0 || strcasecmp(str, "true") == 0 ||
	    strcasecmp(str, "on") == 0 || strcasecmp(str, "set") == 0) {
		*out = true;
		return true;
	}
	if (strcasecmp(str, "0") == 0 || strcasecmp(str, "false") == 0 ||
	    strcasecmp(str, "off") == 0 || strcasecmp(str, "clear") == 0) {
		*out = false;
		return true;
	}
	return false;
}

static bool entity_flag_matches(const char *input, const char *base_name) {
	char full_name[128];
	int n = snprintf(full_name, sizeof(full_name), "ENTITY_FLAG_%s", base_name);
	if (n < 0 || n >= (int)sizeof(full_name)) {
		return false;
	}
	return strcasecmp(input, base_name) == 0 || strcasecmp(input, full_name) == 0;
}

// Usage:
//   set flag <FLAG> [1|0|on|off|true|false]   (defaults to enabling if omitted)
//   set flag none                             (clears all flags)
static void set_prototype_flag(int argc, char *argv[]) {
	if (argc < 3 || argc > 4) {
		printf("Invalid arguments for set flag, requires FLAG [1|0|on|off]\n");
		return;
	}

	const char *flag_arg = argv[2];
	if (strcasecmp(flag_arg, "none") == 0 || strcasecmp(flag_arg, "ENTITY_FLAG_NONE") == 0) {
		prototype.flags = ENTITY_FLAG_NONE;
		printf("Cleared all entity flags\n");
		return;
	}

	bool enable = true;
	if (argc == 4 && !parse_bool_flag(argv[3], &enable)) {
		printf("Invalid flag state '%s' (expected 1/0, on/off, true/false)\n", argv[3]);
		return;
	}

	#define X(fname, bit) \
		if (entity_flag_matches(flag_arg, #fname)) { \
			if (enable) { \
				prototype.flags = (u8)(prototype.flags | ENTITY_FLAG_##fname); \
			} else { \
				prototype.flags = (u8)(prototype.flags & ~ENTITY_FLAG_##fname); \
			} \
			printf("%s ENTITY_FLAG_%s (flags = 0x%02X)\n", \
				enable ? "Set" : "Cleared", #fname, (unsigned)prototype.flags); \
			return; \
		}
	ENTITY_FLAG_LIST
	#undef X

	printf("Unknown entity flag '%s'\n", flag_arg);
}

static bool item_type_matches(const char *input, const char *base_name) {
	char full_name[128];
	int n = snprintf(full_name, sizeof(full_name), "ITEM_%s", base_name);
	if (n < 0 || n >= (int)sizeof(full_name)) {
		return false;
	}
	return strcasecmp(input, base_name) == 0 || strcasecmp(input, full_name) == 0;
}

static bool parse_item_type(const char *input, enum ItemType *out_type) {
	#define X(iname) \
		if (item_type_matches(input, #iname)) { \
			*out_type = ITEM_##iname; \
			return true; \
		}
	ITEM_LIST
	#undef X
	return false;
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
// Usage:
//   set inventory <SLOT 0..31> <ITEM|none> [COUNT]
//   set hotbar    <SLOT 0..7>  <ITEM|none> [COUNT]
static void set_prototype_container_slot(bool is_hotbar, int argc, char *argv[]) {
	const char *area = is_hotbar ? "hotbar" : "inventory";
	u32 max_slots = is_hotbar ? HOTBAR_SIZE : INVENTORY_SIZE;
	struct ItemStack *slots = is_hotbar
		? prototype.starting_inventory.hotbar_items
		: prototype.starting_inventory.items;

	if (argc < 4 || argc > 5) {
		printf("Invalid arguments for set %s, requires SLOT ITEM [COUNT]\n", area);
		return;
	}

	u32 slot = 0;
	if (!parse_u32(argv[2], &slot) || slot >= max_slots) {
		printf("Invalid %s slot '%s' (must be 0..%u)\n", area, argv[2], max_slots - 1u);
		return;
	}

	if (strcasecmp(argv[3], "none") == 0 || strcasecmp(argv[3], "clear") == 0 || strcasecmp(argv[3], "empty") == 0) {
		slots[slot].id = 0;
		slots[slot].count = 0;
		slots[slot].active = false;
		printf("Cleared %s slot [%u]\n", area, slot);
		return;
	}

	enum ItemType item;
	if (!parse_item_type(argv[3], &item)) {
		printf("Unknown item '%s'\n", argv[3]);
		return;
	}

	u32 count = 1;
	if (argc == 5) {
		if (!parse_u32(argv[4], &count) || count == 0) {
			printf("Invalid item count '%s' (must be >= 1)\n", argv[4]);
			return;
		}
	}

	slots[slot].id = (u32)item;
	slots[slot].count = count;
	slots[slot].active = true;

	printf("Set %s slot [%u] = ITEM_%s (id=%u, count=%u)\n",
		area, slot,
		#define X(iname) (item == ITEM_##iname) ? #iname :
		ITEM_LIST
		#undef X
		"UNKNOWN",
		slots[slot].id, slots[slot].count);
}
// Helper to case-insensitively match a field name like "strength" or "ST_STRENGTH"
static const char *entity_type_to_name(enum EntityType t) {
	switch (t) {
		#define X(ename) case ename##_ENTITY: return #ename "_ENTITY";
		ENTITY_TYPE_LIST
		#undef X
		default: return "UNKNOWN";
	}
}

static void list_prototype(void) {
	printf("=== EntityPrototype ===\n");
	printf("  name:           %s\n", (name && name[0]) ? name : "(none)");
	printf("  type:           %s (%d)\n", entity_type_to_name(prototype.type), (int)prototype.type);
	printf("  starting_level: %u\n", prototype.starting_level);
	printf("  flags:          0x%02X\n", (unsigned)prototype.flags);
	printf("  dialogue_path:  %s\n", (prototype.dialogue_path && prototype.dialogue_path[0]) ? prototype.dialogue_path : "(none)");
	printf("  collision:      x=%u, y=%u, w=%u, h=%u\n",
		prototype.collision.x, prototype.collision.y,
		prototype.collision.w, prototype.collision.h);

	printf("  -- Base Stats --\n");
	#define X(stat_name) \
		printf("    %-14s = %d\n", #stat_name, (int)prototype.stats.base_stats[ST_##stat_name]);
	STATS_LIST
	#undef X

	printf("  -- Skill Mods --\n");
	#define X(skill_name) \
		printf("    %-14s = %d\n", #skill_name, (int)prototype.stats.skill_mod[SK_##skill_name]);
	SKILLS_LIST
	#undef X

	printf("  -- Hotbar (active / non-zero) --\n");
	for (int i = 0; i < HOTBAR_SIZE; i++) {
		const struct ItemStack *s = &prototype.starting_inventory.hotbar_items[i];
		if (s->active || s->id != 0 || s->count != 0) {
			printf("    [%d] id=%u, count=%u, active=%s\n",
				i, s->id, s->count, s->active ? "true" : "false");
		}
	}

	printf("  -- Inventory (active / non-zero) --\n");
	for (int i = 0; i < INVENTORY_SIZE; i++) {
		const struct ItemStack *s = &prototype.starting_inventory.items[i];
		if (s->active || s->id != 0 || s->count != 0) {
			printf("    [%d] id=%u, count=%u, active=%s\n",
				i, s->id, s->count, s->active ? "true" : "false");
		}
	}
}

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
			printf("Out of memory setting name\n");
			return;
		}
		free(name);
		name = copy;
		printf("Set name = %s\n", name);
		return;
	}
	if (strcasecmp(field, "type") == 0) {
		#define X(tname) \
			if (entity_type_matches(value, #tname)) { \
				prototype.type = tname##_ENTITY; \
				printf("Set type = %s\n", #tname "_ENTITY"); \
				return; \
			}
		ENTITY_TYPE_LIST
		#undef X

		printf("Unknown entity type '%s'\n", value);
		return;
	}

	if (strcasecmp(field, "level") == 0 || strcasecmp(field, "starting_level") == 0) {
		u32 lvl;
		if (!parse_u32(value, &lvl)) {
			printf("Invalid level value '%s'\n", value);
			return;
		}
		prototype.starting_level = lvl;
		printf("Set starting_level = %u\n", prototype.starting_level);
		return;
	}

	if (strcasecmp(field, "dialogue") == 0 || strcasecmp(field, "dialogue_path") == 0) {
		// Note: if you allocate dialogue_path dynamically, track/free it in reset_prototype()
		prototype.dialogue_path = strdup(value);
		printf("Set dialogue_path = %s\n", prototype.dialogue_path);
		return;
	}

	// Match base stats from STATS_LIST (e.g., "strength", "ST_STRENGTH", "agility")
	#define X(stat_name) \
		if (field_matches(field, #stat_name, "ST_")) { \
			s16 v; \
			if (!parse_s16(value, &v)) { \
				printf("Invalid integer for stat '%s': %s\n", field, value); \
				return; \
			} \
			prototype.stats.base_stats[ST_##stat_name] = v; \
			printf("Set base stat %s = %d\n", #stat_name, v); \
			return; \
		}
	STATS_LIST
	#undef X

	// Match skill mods from SKILLS_LIST (e.g., "guns", "SK_GUNS", "sneak")
	#define X(skill_name) \
		if (field_matches(field, #skill_name, "SK_")) { \
			s16 v; \
			if (!parse_s16(value, &v)) { \
				printf("Invalid integer for skill '%s': %s\n", field, value); \
				return; \
			} \
			prototype.stats.skill_mod[SK_##skill_name] = v; \
			printf("Set skill mod %s = %d\n", #skill_name, v); \
			return; \
		}
	SKILLS_LIST
	#undef X

	printf("Unknown prototype field '%s'\n", field);
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
	int written = snprintf(filename, sizeof(filename), "assets/prototype/%s.proto", base_name);
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
		printf(
			"Available Commands:\n"
			"  help: show this message\n"
			"  exit/quit: exit program\n"
			"  set FIELD VALUE: set a field on prototype (name, type, level, dialogue, stats, skills)\n"
			"  set flag FLAG [1|0|on|off|true|false]|none: toggle or clear entity flags\n"
			"  set inventory SLOT ITEM|none [COUNT]: set or clear inventory slot (0..31)\n"
			"  set hotbar SLOT ITEM|none [COUNT]: set or clear hotbar slot (0..7)\n"
			"  write NAME: write prototype to assets/prototype/NAME.proto\n"
			"  open/read NAME: load prototype from NAME.proto\n"
			"  list: display current prototype fields\n"
			"  reset: clears the prototype\n"
		);
	} else if (strcmp(argv[0], "set") == 0) {
		if (argc < 2) {
			printf("Invalid arguments for set, requires FIELD VALUE\n");
			return;
		}
		if (strcasecmp(argv[1], "flag") == 0 || strcasecmp(argv[1], "flags") == 0) {
			set_prototype_flag(argc, argv);
			return;
		}
		if (strcasecmp(argv[1], "inventory") == 0 || strcasecmp(argv[1], "inv") == 0) {
			set_prototype_container_slot(false, argc, argv);
			return;
		}
		if (strcasecmp(argv[1], "hotbar") == 0) {
			set_prototype_container_slot(true, argc, argv);
			return;
		}
		if (argc != 3) {
			printf("Invalid arguments for set, requires FIELD VALUE\n");
			return;
		}
		set_prototype_field(argv[1], argv[2]);
	} else if (strcmp(argv[0], "write") == 0) {
		if (argc != 2) {
			printf("Invalid arguments for write, requires NAME\n");
			return;
		}
		write_prototype_file(argv[1]);
	} else if (strcmp(argv[0], "reset") == 0) {
		reset_prototype();
		printf("Prototype reset\n");
	} else if (strcmp(argv[0], "open") == 0 || strcmp(argv[0], "read") == 0) {
		if (argc != 2) {
			printf("Invalid arguments for %s, requires NAME\n", argv[0]);
			return;
		}
		struct EntityPrototype *loaded = open_prototype_file(argv[1], PROTO_MAGIC, PROTO_VERSION);
		if (loaded) {
			free((void *)prototype.dialogue_path);
			prototype = *loaded;
			free(loaded);
		}
	} else if (strcmp(argv[0], "list") == 0) {
		if (argc != 1) {
			printf("Invalid arguments for list\n");
			return;
		}
		list_prototype();
	} else {
		printf("Unknown command: %s. Type help for help\n", argv[0]);
	}
}

bool run_cli(){
	printf("Entering CLI mode. Type 'help' or 'exit' to quit\n");

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
