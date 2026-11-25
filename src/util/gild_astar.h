#pragma once
#include "gild_basic.h"
#include "gild_math.h"
// Astar takes a 2d heap array of u8s. If the first bit of the u8 is flipped it's collision, if it's not then it's walkable.
struct Gild_AstarPath{
	gild_v2_u32 *path;
	u32 path_length;		
};

bool gild_pathfind(u8 *m, u32 w, u32 h,gild_v2_u32 start, gild_v2_u32 target,struct Gild_AstarPath *out);
// Same, but a tile blocks if any bit in block_mask is set (gild_pathfind uses 1).
// The start tile is never checked, so a character can path away from its own tile.
bool gild_pathfind_mask(u8 *m, u32 w, u32 h, gild_v2_u32 start, gild_v2_u32 target, u8 block_mask, struct Gild_AstarPath *out);
