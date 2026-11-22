#include <stdlib.h>
#include <string.h>
#include "gild_basic.h"
#include "gild_hash.h"
#include "gild_mem.h"
#include "gild_log.h"
#include "gild_io.h"

// Open addressing with linear probing: every entry lives directly in the
// slots array. A name starts at slot (hash & (array_size - 1)); if that slot
// holds a different name, the next slot is tried, wrapping at the end.
//
// The array is sized so it is at most 75% full and ALWAYS has at least one
// empty slot (array_size > entry_count, and adding past entry_count is
// refused). That empty slot is what stops a lookup of a missing name.
//
// Note for a future gild_map_remove: a removed slot can't simply be set back
// to empty, because names that probed past it would become unreachable. Use a
// "deleted" marker (skipped by lookups, reusable by inserts) or shift the
// following entries back.

#define GILD_MAP_MAX_ENTRIES (1u << 30)   // keeps array_size within u32

static u32 pwr2(u32 n);

// Length of `name` if it fits in a slot (1..GILD_MAP_NAME_MAX-1 chars),
// otherwise logs and returns 0. memchr only looks at the first
// GILD_MAP_NAME_MAX bytes, so a very long name is never scanned in full.
static size_t checked_name_len(const char *name){
	const char *end = memchr(name, '\0', GILD_MAP_NAME_MAX);
	if(!end){
		GILD_LOG(GILD_TRUNCATED, "Map key \"%.15s...\" is longer than %d characters", name, GILD_MAP_NAME_MAX - 1);
		return 0;
	}
	if(end == name){
		GILD_LOG(GILD_INVALID_ARG, "Map key can't be an empty string");
		return 0;
	}
	return (size_t)(end - name);
}

// Returns the slot holding `name`, or the empty slot where it would go.
static struct Gild_MapSlot *find_slot(const struct Gild_HashMap *m, const char *name, u64 hash){
	u32 mask = m->array_size - 1;
	for(u32 i = (u32)(hash & mask);; i = (i + 1) & mask){
		struct Gild_MapSlot *s = &m->slots[i];
		if(s->name[0] == '\0'){return s;}
		if(s->hash == hash && strcmp(s->name, name) == 0){return s;}
	}
}

// Can't grow
struct Gild_HashMap* gild_map_make(const u32 entry_count){
	if(entry_count > GILD_MAP_MAX_ENTRIES){
		GILD_LOG(GILD_INVALID_ARG, "Map entry count %u is too large (max %u)", entry_count, GILD_MAP_MAX_ENTRIES);
		return NULL;
	}
	struct Gild_HashMap *m = GILD_XCALLOC(1, sizeof(struct Gild_HashMap));

	m->entry_count = entry_count;
	m->array_size  = pwr2(entry_count + entry_count / 3 + 1);   // <= 75% full, never completely full
	m->slots       = GILD_XCALLOC(m->array_size, sizeof(struct Gild_MapSlot));

	return m;
}
bool gild_map_add(struct Gild_HashMap *m, void *data, const char *name){
	if(!m || !data || !name){GILD_LOG(GILD_IS_NULL, "Required arguments is NULL"); return false;}
	if(!m->slots){return false;}

	size_t len = checked_name_len(name);
	if(len == 0){return false;}

	u64 hash = gild_murmurhash64a(name, len, 0);
	struct Gild_MapSlot *s = find_slot(m, name, hash);

	if(s->name[0] != '\0'){
		GILD_LOG(GILD_DUPLICATE, "Found duplicate for %s", name);
		// Overwrite the existing data for this key; the map owns the old value.
		if(s->data != data){free(s->data);}
		s->data = data;
		return true;
	}

	// Checked after the lookup so overwriting a key still works when full.
	if(m->actual_entry_count >= m->entry_count){
		GILD_LOG(GILD_FULL, "Cannot add %s, map is full (%u entries). Reminder, GILD MAPS are a fixed size at initialization.", name, m->entry_count);
		return false;
	}

	s->hash = hash;
	memcpy(s->name, name, len + 1);   // + 1 copies the '\0' too
	s->data = data;
	m->actual_entry_count++;
	return true;
}
bool gild_map_get(struct Gild_HashMap *m, void *out, const char *name){
	if(!m || !name){GILD_LOG(GILD_IS_NULL, "Map or Name is NULL"); return false;}
	if(!m->slots){return false;}

	size_t len = checked_name_len(name);
	if(len == 0){return false;}

	struct Gild_MapSlot *s = find_slot(m, name, gild_murmurhash64a(name, len, 0));
	if(s->name[0] == '\0'){return false;}
	if(out){*(void **)out = s->data;}
	return true;
}
void gild_map_free(struct Gild_HashMap *m){
	if(!m){return;}
	if(m->slots){
		for(u32 i = 0; i < m->array_size; i++){
			free(m->slots[i].data);   // free(NULL) is a no-op for empty slots
		}
		free(m->slots);
	}
	free(m);
}
// ---------------------------------------------------------------------------
// Saving and loading. Layout written by gild_map_write (all little-endian):
//
//   u32  format version (GILD_MAP_FORMAT_VERSION)
//   u32  entry_count          (the map's capacity)
//   u32  actual_entry_count   (number of entries that follow)
//   per entry:
//     u8   key length (1..GILD_MAP_NAME_MAX-1)
//     key bytes (no '\0')
//     the value, as written by your writer function
//
// gild_map_save puts a gild_write_header() in front of that.
// ---------------------------------------------------------------------------
#define GILD_MAP_FORMAT_VERSION 1u

bool gild_map_write(FILE *f, const struct Gild_HashMap *m, Gild_MapValueWriter write_value, void *user){
	if(!f || !m || !write_value){GILD_LOG(GILD_IS_NULL, "Passed NULL file, map or writer to gild_map_write"); return false;}
	if(!m->slots){return false;}

	if(!(gild_write_u32(f, GILD_MAP_FORMAT_VERSION) &&
	     gild_write_u32(f, m->entry_count) &&
	     gild_write_u32(f, m->actual_entry_count))){
		GILD_LOG(GILD_WRITE, "Failed to write map");
		return false;
	}

	u32 written = 0;
	for(u32 i = 0; i < m->array_size; i++){
		const struct Gild_MapSlot *s = &m->slots[i];
		if(s->name[0] == '\0'){continue;}

		u8 len = (u8)strlen(s->name);   // always < GILD_MAP_NAME_MAX
		if(!(gild_write_u8(f, len) && gild_write_bytes(f, s->name, len))){
			GILD_LOG(GILD_WRITE, "Failed to write map key %s", s->name);
			return false;
		}
		if(!write_value(f, s->data, user)){
			GILD_LOG(GILD_WRITE, "Value writer failed for map key %s", s->name);
			return false;
		}
		written++;
	}
	if(written != m->actual_entry_count){
		GILD_LOG(GILD_CORRUPT, "Map holds %u entries but says %u", written, m->actual_entry_count);
		return false;
	}
	return true;
}

struct Gild_HashMap *gild_map_read(FILE *f, Gild_MapValueReader read_value, void *user){
	if(!f || !read_value){GILD_LOG(GILD_IS_NULL, "Passed NULL file or reader to gild_map_read"); return NULL;}

	u32 format, capacity, count;
	if(!(gild_read_u32(f, &format) && gild_read_u32(f, &capacity) && gild_read_u32(f, &count))){
		GILD_LOG(GILD_READ, "Map data is missing or cut short");
		return NULL;
	}
	if(format != GILD_MAP_FORMAT_VERSION){
		GILD_LOG(GILD_VERSION_MISMATCH, "Map format %u, expected %u", format, GILD_MAP_FORMAT_VERSION);
		return NULL;
	}
	// Checked before allocating anything, so a damaged file can't request a huge map.
	if(capacity > GILD_MAP_MAX_ENTRIES || count > capacity){
		GILD_LOG(GILD_CORRUPT, "Map data has impossible sizes (capacity %u, entries %u)", capacity, count);
		return NULL;
	}

	struct Gild_HashMap *m = gild_map_make(capacity);
	for(u32 i = 0; i < count; i++){
		u8 len;
		char name[GILD_MAP_NAME_MAX];
		if(!gild_read_u8(f, &len) || len == 0 || len >= GILD_MAP_NAME_MAX ||
		   !gild_read_bytes(f, name, len) || memchr(name, '\0', len)){
			GILD_LOG(GILD_CORRUPT, "Bad key in map data (entry %u)", i);
			goto fail;
		}
		name[len] = '\0';

		void *data = read_value(f, user);
		if(!data){
			GILD_LOG(GILD_READ, "Value reader failed for map key %s", name);
			goto fail;
		}
		if(gild_map_get(m, NULL, name)){
			GILD_LOG(GILD_CORRUPT, "Map data contains key %s twice", name);
			free(data);
			goto fail;
		}
		if(!gild_map_add(m, data, name)){
			free(data);
			goto fail;
		}
	}
	return m;

fail:
	gild_map_free(m);   // also frees every value read so far
	return NULL;
}

bool gild_map_save(const struct Gild_HashMap *m, const char *path,
                   const char magic[8], u32 version,
                   Gild_MapValueWriter write_value, void *user){
	if(!m || !path || !magic || !write_value){GILD_LOG(GILD_IS_NULL, "Passed NULL argument to gild_map_save"); return false;}

	FILE *f = gild_fopen(path, "wb");
	if(!f){GILD_LOG(GILD_OPEN, "Failed to open %s for writing", path); return false;}

	bool ok = gild_write_header(f, magic, version) && gild_map_write(f, m, write_value, user);
	// fclose flushes buffered data, so a full disk can show up only here.
	if(fclose(f) != 0){
		GILD_LOG(GILD_WRITE, "Failed to finish writing %s", path);
		ok = false;
	}
	return ok;
}

struct Gild_HashMap *gild_map_load(const char *path,
                                   const char magic[8], u32 version,
                                   Gild_MapValueReader read_value, void *user){
	if(!path || !magic || !read_value){GILD_LOG(GILD_IS_NULL, "Passed NULL argument to gild_map_load"); return NULL;}

	FILE *f = gild_fopen(path, "rb");
	if(!f){GILD_LOG(GILD_NO_FILE, "Failed to open %s", path); return NULL;}

	struct Gild_HashMap *m = NULL;
	if(gild_read_header(f, magic, version)){
		m = gild_map_read(f, read_value, user);
	}
	fclose(f);
	return m;
}

// MurmurHash64A by Austin Appleby (public domain).
// Original: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
// Ported to C and gild_basic types. Block reads are done bytewise
// (little-endian), so there is no unaligned access UB and hashes are
// identical on every platform.

static u64 load_u64_le(const u8 *p){
	return  (u64)p[0]        | ((u64)p[1] << 8)  | ((u64)p[2] << 16) | ((u64)p[3] << 24) |
	      ((u64)p[4] << 32) | ((u64)p[5] << 40) | ((u64)p[6] << 48) | ((u64)p[7] << 56);
}

u64 gild_murmurhash64a(const void *key, size_t len, u64 seed){
	const u64 m = 0xc6a4a7935bd1e995ULL;
	const u32 r = 47;

	u64 h = seed ^ ((u64)len * m);

	const u8 *data = (const u8 *)key;
	const u8 *end  = data + (len & ~(size_t)7);

	while(data != end){
		u64 k = load_u64_le(data);
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
