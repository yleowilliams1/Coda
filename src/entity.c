#include "entity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/gild_log.h"
#include "util/gild_io.h"
#include "inventory.h"
#include "util/gild_mem.h"
#include "stats.h"

static bool read_string_field(FILE *f, char **out_str) {
	if (!f || !out_str) return false;

	u32 len = 0;
	if (!gild_read_u32(f, &len)) {
		return false;
	}

	// Guard against corrupted/huge length values
	if (len > 4096u) {
		GILD_LOG(GILD_INVALID_FORMAT, "String field length too large: %u", len);
		return false;
	}

	char *buf = malloc((size_t)len + 1u);
	if (!buf) {
		GILD_LOG(GILD_IS_NULL, "Out of memory reading string field");
		return false;
	}

	if (len > 0 && !gild_read_bytes(f, buf, len)) {
		free(buf);
		return false;
	}

	buf[len] = '\0';
	*out_str = buf;
	return true;
}

static bool read_item_stack(FILE *f, struct ItemStack *stack) {
	u8 active_byte = 0;
	if (!gild_read_u32(f, &stack->id)
	    || !gild_read_u32(f, &stack->count)
	    || !gild_read_u8(f, &active_byte)) {
		return false;
	}
	stack->active = (active_byte != 0u);
	return true;
}

static void build_proto_path(const char *arg, char *out, size_t out_size) {
	size_t arg_len = strlen(arg);
	const char *ext = ".proto";
	size_t ext_len = strlen(ext);

	if (arg_len >= ext_len && strcmp(arg + arg_len - ext_len, ext) == 0) {
		snprintf(out, out_size, "%s", arg);
	} else {
		snprintf(out, out_size, "%s.proto", arg);
	}
}

struct EntityPrototype *open_prototype_file(const char *arg, const char mag[8], u32 ver) {
	if (!arg || arg[0] == '\0') {
		GILD_LOG(GILD_INVALID_ARG, "Invalid file name");
		return NULL;
	}

	char filename[512];
	build_proto_path(arg, filename, sizeof(filename));

	FILE *fp = gild_fopen(filename, "rb");
	if (!fp) {
		GILD_LOG(GILD_NO_FILE, "Failed to open '%s' for reading", filename);
		return NULL;
	}

	if (!gild_read_header(fp, mag, ver)) {
		fclose(fp);
		return NULL;
	}

	struct EntityPrototype next_proto;
	memset(&next_proto, 0, sizeof(next_proto));
	char *next_name = NULL;
	char *next_dialogue = NULL;
	u32 type_u32 = 0;
	u8 flags_u8 = 0;

	bool ok = read_string_field(fp, &next_name)
	       && gild_read_u32(fp, &type_u32)
	       && gild_read_u32(fp, &next_proto.starting_level)
	       && gild_read_u8(fp, &flags_u8)
	       && read_string_field(fp, &next_dialogue)
	       && gild_read_u32(fp, &next_proto.collision.x)
	       && gild_read_u32(fp, &next_proto.collision.y)
	       && gild_read_u32(fp, &next_proto.collision.w)
	       && gild_read_u32(fp, &next_proto.collision.h);

	if (ok) {
		if (type_u32 >= ENTITY_TYPE_COUNT) {
			GILD_LOG(GILD_INVALID_FORMAT, "Invalid entity type %u in '%s'", type_u32, filename);
			ok = false;
		} else {
			next_proto.type = (enum EntityType)type_u32;
			next_proto.flags = flags_u8;
			next_proto.dialogue_path = next_dialogue;
		}
	}

	for (int i = 0; ok && i < ST_STAT_COUNT; i++) {
		ok = gild_read_s16(fp, &next_proto.stats.base_stats[i]);
	}

	for (int i = 0; ok && i < SK_SKILL_COUNT; i++) {
		ok = gild_read_s16(fp, &next_proto.stats.skill_mod[i]);
	}

	for (int i = 0; ok && i < HOTBAR_SIZE; i++) {
		ok = read_item_stack(fp, &next_proto.starting_inventory.hotbar_items[i]);
	}

	for (int i = 0; ok && i < INVENTORY_SIZE; i++) {
		ok = read_item_stack(fp, &next_proto.starting_inventory.items[i]);
	}

	fclose(fp);
	free(next_name);

	if (!ok) {
		free(next_dialogue);
		GILD_LOG(GILD_READ, "Failed to parse prototype from '%s'", filename);
		return NULL;
	}

	GILD_LOG(GILD_INFO, "Loaded prototype from '%s'", filename);

	struct EntityPrototype *proto = GILD_XCALLOC(1, sizeof(*proto));
	*proto = next_proto;
	return proto;
}
