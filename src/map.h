#pragma once
#include "util/gild_math.h"
#include "util/gild_basic.h"

// ---------------------------------------------------------------------------
// Isometric layout. World coordinates are raylib's: pixels, (0,0) top-left, y down.
//
// A tile is a 64x32 diamond. Every texture is 64x96: the diamond is the bottom
// 32 rows and the space above it is for walls and ceilings. All textures are drawn
// with the same origin, (32, 80) from the texture's top-left = the diamond's centre,
// placed on the tile's anchor.
//
// Tile axes: tile (0,0) is the top corner of the map. +tx goes down-right, +ty goes
// down-left. floors[] and flags[] are indexed ty * w + tx.
//
//   anchor.x = (tx - ty + h) * 32     (the +h keeps every anchor x >= 32 on the map,
//   anchor.y = (tx + ty + 1) * 16      so pixel positions always fit in a u32)
//
// Because of the +h, pixel positions depend on the map's height: always go through
// map_tile_to_world / map_world_to_tile rather than doing the maths by hand.
// ---------------------------------------------------------------------------
#define TILE_W 64
#define TILE_H 32
#define TEX_ORIGIN_X 32
#define TEX_ORIGIN_Y 80
#define MAP_TEXTURE_PATH "assets/textures/%d.png"

// Largest w * h accepted when creating or loading a map.
#define MAP_MAX_TILES (1u << 22)

// Tile flags (one byte per tile in Map.flags)
#define MAP_FLAG_COLLIDE (1u << 0)

enum MapEntityType{
	PLAYER,
	NPC,
	DOOR,
	LOOT,
	PROP,
};

// pos is the entity's anchor in world pixels (see above). The editor snaps it to a
// tile anchor on creation; the entity manager moves it freely afterwards.
struct MapEntity{
	const char *prototype;
	enum MapEntityType type;
	u32 uid;
	gild_v2_u32 pos;
	u8 sprite;
	u8 should_collide;
};
// Rectangle in tile coordinates.
struct MapCeiling {
	u16 x, y, w, h;
	u8 sprite;
};
struct Map{
	struct MapEntity *entities;
	u16 entity_count;
	u16 w, h;

	struct MapCeiling *ceilings;
	u16 ceiling_count;

	u8 *flags;
	u8 *floors;

	// Filled in by draw_map every frame (index of the PLAYER entity), not saved.
	u8 player_state;
	u32 player;
};

void update_entity(struct Map *m, u32 uid, gild_v2_u32 new_pos, u8 new_sprite);

// Draws floors back to front with the entities y-sorted in between, so walls on
// nearer tiles cover entities behind them, then the ceilings (except the one the
// player is under). Sorts m->entities in place.
void draw_map(struct Map *m);

// Anchor of a tile in world pixels. Only valid for tiles inside the map.
gild_v2_u32 map_tile_to_world(const struct Map *m, u32 tx, u32 ty);
// Same, for fractional tile coordinates (tile centres are whole numbers, so
// tx +- 0.5 / ty +- 0.5 are the diamond's corners). Works outside the map too.
void map_tile_to_worldf(const struct Map *m, float tx, float ty, float *x, float *y);
// Tile under a world-pixel point. Always writes the tile (it can be outside the map
// or negative) and returns whether it is inside the map.
bool map_world_to_tile(const struct Map *m, float x, float y, s32 *tx, s32 *ty);

// Texture cache shared by everything that draws map art. Loads on first use and
// returns NULL if MAP_TEXTURE_PATH for that id doesn't exist (checked only once).
// Needs a window. Pointer type is raylib's Texture2D.
struct Texture;
const struct Texture *map_texture(u8 id);
