#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include "gild_mem.h"
#include "gild_hash.h"
#include "gild_basic.h"
#include "gild_log.h"

#define GROWTH 50

static u64 gild_murmurhash64a(const void *key, size_t len, u64 seed);
static u32 pwr2(u32 n);
static void _grow_shrink(struct Gild_PerfectHash *h, u32 count){
	if(!h){return;}
	h->data_indexs = GILD_XREALLOC(h->data_indexs, count * sizeof(u32));
	h->name_offsets = GILD_XREALLOC(h->name_offsets, count * sizeof(u32));
	h->data = GILD_XREALLOC(h->data, count * sizeof(u32));
	h->name = GILD_XREALLOC(h->name, count * sizeof(char));
	h->entry_allocated = count;
}
struct Gild_PerfectHash *gild_create_phash(u32 count){
	if(count < GROWTH){count = GROWTH;}
	if(count > GROWTH){GILD_LOG(GILD_IS_NULL, "Count exceeds 100");return NULL;}

	struct Gild_PerfectHash *h = GILD_XCALLOC(1, sizeof(struct Gild_PerfectHash));
	h->entry_allocated = count;

	h->data_indexs = GILD_XCALLOC(count, sizeof(u32));
	h->name_offsets = GILD_XCALLOC(count, sizeof(u32));
	
	h->data = GILD_XCALLOC(count, sizeof(u32));
	h->name = GILD_XCALLOC(count, sizeof(char));
	
	return h;
}
bool gild_free_phash(struct Gild_PerfectHash *h){
	if(!h){return false;}
	if(h->data_indexs){free(h->data_indexs);}
	if(h->name_offsets){free(h->name_offsets);}
	if(h->data){free(h->data);}
	if(h->name){free(h->name);}
	free(h);
	return true;
}
bool gild_add_phash(struct Gild_PerfectHash *h, u32 data, u32 str_len, const char *name){
	if (!h || !name) {GILD_LOG(GILD_IS_NULL, "Required arguments is NULL");return false;}
	if (str_len == 0) {GILD_LOG(GILD_INVALID_ARG, "Map key can't be empty");return false;}
	if (str_len >= GILD_MAP_NAME_MAX) {GILD_LOG(GILD_TRUNCATED, "Map key is longer than %d characters", GILD_MAP_NAME_MAX - 1);return false;}
	
	if (h->entries_used + 1 >= h->entry_allocated){_grow_shrink(h, h->entries_used + GROWTH);}
	if(!h->data_indexs || !h->name_offsets || !h->data || !h->name){return false;}

	h->data_indexs[h->entries_used] = h->entries_used;
	h->data[h->entries_used] = data;
	h->name_offsets[h->entries_used] = h->name_size;

	memcpy(h->name + h->name_size, name, str_len);

	h->name_size += str_len;	
	h->entries_used++;
	return true;
}
static inline uint64_t next_seed64(uint64_t *state) {
	uint64_t x = (*state += 0x9e3779b97f4a7c15ULL);
	x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
	x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
	return x ^ (x >> 31);
}
#define UNDETERMINED 0
#define COLLIDE 1
#define OUT_OF_INTERACTIONS 2
#define PERFECT 3
struct Gild_StaticMap *gild_create_map(struct Gild_PerfectHash *h){
	struct Gild_StaticMap *m = GILD_XCALLOC(1, sizeof(struct Gild_StaticMap));	

	m->count = h->entries_used;
	
	// Copy the arena blocks over
	m->arena = GILD_XCALLOC(h->entries_used, sizeof(u32));
	memcpy(m->arena, h->data, h->entries_used * sizeof(u32));

	m->names = GILD_XCALLOC(1, h->name_size);
	memcpy(m->names, h->name, h->name_size);	
	m->name_size = h->name_size;

	// Allocated 75%	
	m->allocated = pwr2(m->count + m->count / 3 + 1);   
	m->offsets = GILD_XCALLOC(m->allocated, sizeof(struct OffsetItem));
	
	// Now find perfect hash
	uint64_t seed = (uint64_t)time(NULL) ^ 0xa0761d6478bd642fULL;
	GILD_LOG(GILD_INFO, "Starting seed %PRIu64", seed);

	u64 *buffer = GILD_XCALLOC(m->count, sizeof(u64));

	bool found = false;
	u64 perfect_hash;
	for(u32 attempt = 0; attempt < MAX_ITEMS; attempt++){
		u64 candidate = next_seed64(&seed);
		bool collided = false;
			
		for(u32 i = 0; i < m->count; i++){
			buffer[i] = gild_murmurhash64a(m->names + m->offsets[i].name_offset, m->offsets[i].name_offset - m->offsets[i + 1].name_offset, candidate) & (m->allocated - 1);
			for(u32 j = 0; j < i; j++){
				if(buffer[i] == buffer[j]){
					collided = true;
					break;
				}
			}
			if(collided){break;}
		}
		if(!collided){
			// Found candidate
			perfect_hash = candidate;
			found = true;
			break;
		}
	}
	if(!found){
		GILD_LOG(GILD_IS_NULL, "Failed to find perfect hash. Try decreasing number of items");
		return m;
	}
	return m;
}
bool gild_map_add(struct Gild_u32Map *m, u32 data, u32 str_len, const char *name){
	if (!m || !name) {GILD_LOG(GILD_IS_NULL, "Required arguments is NULL");return false;}
	if (str_len == 0) {GILD_LOG(GILD_INVALID_ARG, "Map key can't be empty");return false;}
	if (str_len >= GILD_MAP_NAME_MAX) {GILD_LOG(GILD_TRUNCATED, "Map key is longer than %d characters", GILD_MAP_NAME_MAX - 1);return false;}
	if (m->used >= m->count) {GILD_LOG(GILD_OUT_OF_BOUNDS, "Exceeded limit");return false;}


	if (m->offsets[index].occupied) {
		GILD_LOG(GILD_OVERFLOW, "Something broke, either we hashed to the same value or the same name was used: %.*s", (int)str_len, name);
		return false;
	}

	// Store then increment so the first item starts at index/offset 0 cleanly.
	m->offsets[index].arena_idx = m->arena_idx_cursor++;
	m->offsets[index].string_offset = m->name_offset_cursor;
	m->offsets[index].name_len = str_len;
	m->offsets[index].occupied = true;

	m->name_offset_cursor += str_len;
	m->names = GILD_XREALLOC(m->names, m->name_offset_cursor);

	memcpy(m->names + m->offsets[index].string_offset, name, str_len);
	m->arena[m->offsets[index].arena_idx] = data;
	m->used++;
	return true;
}
u64 gild_murmurhash64a(const void *key, size_t len, u64 seed){
	const u64 m = 0xc6a4a7935bd1e995ULL;
	const u32 r = 47;

	u64 h = seed ^ ((u64)len * m);

	const u8 *data = (const u8 *)key;
	const u8 *end  = data + (len & ~(size_t)7);

	while(data != end){
		u64 k = (u64)data[0] | ((u64)data[1] << 8) |((u64)data[2] << 16) | ((u64)data[3] << 24) | ((u64)data[4] << 32) | ((u64)data[5] << 40) | ((u64)data[6] << 48) | ((u64)data[7] << 56);
		data += 8;

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	switch(len & 7){
	case 7: h ^= (u64)data[6] << 48; /* fallthrough */
	case 6: h ^= (u64)data[5] << 40; /* fallthrough */
	case 5: h ^= (u64)data[4] << 32; /* fallthrough */
	case 4: h ^= (u64)data[3] << 24; /* fallthrough */
	case 3: h ^= (u64)data[2] << 16; /* fallthrough */
	case 2: h ^= (u64)data[1] << 8;  /* fallthrough */
	case 1: h ^= (u64)data[0];
	      h *= m;
	}

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}
static u32 pwr2(u32 n) {
	if (n <= 1) return 1;
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1;
}
