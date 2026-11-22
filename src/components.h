#pragma once
#include "util/gild_basic.h"
#include "util/gild_astar.h"
#include "stats.h"
#include "inventory.h"

struct PathfindComponent{
	struct Gild_AstarPath *path;
};
struct StatsComponent{
	enum StatProtoType type;
	struct StatsSaveable stats;
};
struct InventoryComponent{
	struct Inventory inventory;
};
