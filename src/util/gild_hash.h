#pragma once
#include "gild_basic.h"
#define MAX_ITEMS 100
#define GILD_MAP_NAME_MAX 64 


struct Gild_u32Map{
	struct OffsetItem *offsets;

	u32 *arena;
	char *names;
	
	u16 arena_idx_cursor;
	u32 name_offset_cursor;

	u32 count;
	u32 used;
	u32 allocated;
};

struct Gild_PerfectHash{
	u32 *data_indexs;
	u32 *name_offsets;

	u32 entries_used;
	u32 entry_allocated;
	u32 name_size;

	u32 *data;
	char *name;
};
struct OffsetItem{
	u8 item_index;
	u8 occupancy;
	// At 64 max items the max size is 64k below u16 max 65,535
	u16 name_offset;	
};
struct Gild_StaticMap{
	struct OffsetItem *offsets;

	u32 name_size;
	u32 allocated;
	u32 count;

	u32 *arena;
	char *names;
};
