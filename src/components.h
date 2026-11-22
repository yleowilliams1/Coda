#pragma once
#include "util/gild_basic.h"
#include "util/gild_astar.h"
#include "stats.h"
// charactesr are moveable npcs. They can pathfind, have stats, and an inventory and thats it.

struct PathfindComponent{
	struct Gild_AstarPath *path;
};
struct StatsComponent{
	u32 stats_prototype_reference;
	struct StatsSave stats_save;
};
