#pragma once
#include "util/gild_math.h"
#include "util/gild_basic.h"

#define TILE_W 64
#define TILE_H 32
#define TEX_ORIGIN_X 32
#define TEX_ORIGIN_Y 80

#define MAP_MAX_TILES (1u << 22)
#define MAP_MAX_CHARACTERS (1u << 16) // per map; also caps what a corrupt file can ask for
#define MAP_FLAG_COLLIDE  (1u << 0) // wall: set in the editor, saved with the map
#define MAP_FLAG_OCCUPIED (1u << 1) // a character stands here (runtime only: don't save
                                    // it, and clear it when loading a map)

struct MapCeiling {
	u16 x, y, w, h;
	u8 sprite;
};
struct MapTransition {
	u16 x, y, w, h;
	u32 target_id;
	gild_v2_u32 target_pos; 
};
struct MapCharacter{
	u32 uid;
	// Version to load when the save has nothing newer: 0 for a blank character, or a
	// hand-made file "<uid>.<save_index>.sav". Keep these below SAVE_VERSION_FIRST
	// (save_version.h) so game saves can never land on them.
	u32 save_index;
};
struct Map{
	u16 w, h;
	
	u32 *floors;
	u32 *props;

	u32 map_id;
	
	struct MapCharacter *characters;
	u32 character_count;

	struct MapCeiling *ceilings;
	u16 ceiling_count;

	struct MapTransition *transitions;
	u16 transition_count;

	u8 *flags;
};

// Loads or saves data/maps/<map_id>.map. Set m->map_id first (like flag_version).
// Writing: the map is checked first (sizes, rectangles inside the map, no repeated
//   character uid) and refused if it's wrong. It goes to a .tmp file that replaces
//   the old map only once it has written cleanly. MAP_FLAG_OCCUPIED is never saved.
// Reading: *m is only replaced (its old arrays freed) once the whole file has read
//   and checked cleanly, and the file holds the same map_id. On failure *m is
//   untouched. MAP_FLAG_OCCUPIED comes back clear: call character_place for every
//   character afterwards.
bool serialize_map(struct Map *m, bool write);
// Same, with any path (for the editor and tools). Reading takes the file's map_id.
bool serialize_map_path(struct Map *m, const char *path, bool write);
// Frees every array and zeroes the map. Safe on a zeroed or already freed map.
void free_map(struct Map *m);
// "data/maps/<map_id>.map". False if buf is too small.
bool map_path(u32 map_id, char *buf, size_t size);
