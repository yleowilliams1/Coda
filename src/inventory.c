#include "stats.h"
#include "inventory.h"

// This is bad as well, but it works for sure so it's fine for now! I'll setup file io loading and individual item loading eventualyl but for now this is fine.

static struct ItemPrototype PROTOTYPES[ITEM_COUNT] = {
	[ITEM_APPLE] = {0},
	[ITEM_DAGGER] = {0},
	[ITEM_BUTCHERS_KNIFE] = {0},
	[ITEM_LEATHER_JACKET] = {0},
	[ITEM_POWER_ARMOR] = {0},
};

void init_item_protos(){
	PROTOTYPES[ITEM_APPLE].heal_min = 2;
	PROTOTYPES[ITEM_APPLE].heal_max = 6;
	PROTOTYPES[ITEM_APPLE].name = "Apple";
	PROTOTYPES[ITEM_APPLE].value = 2;
	PROTOTYPES[ITEM_APPLE].description = "A red apple healing 2 - 6 HP";

	PROTOTYPES[ITEM_DAGGER].value = 5;
	PROTOTYPES[ITEM_DAGGER].weight = 2;
	PROTOTYPES[ITEM_DAGGER].min_dmg = 2;
	PROTOTYPES[ITEM_DAGGER].max_dmg = 6;
	PROTOTYPES[ITEM_DAGGER].attack_skill_use = SK_MELEE;
	PROTOTYPES[ITEM_DAGGER].heal_min = -2;
	PROTOTYPES[ITEM_DAGGER].heal_max = -6;
	PROTOTYPES[ITEM_DAGGER].name = "Dagger";
	PROTOTYPES[ITEM_DAGGER].description = "A shiny dagger which does 2 - 6 damage. Please don't eat.";

	PROTOTYPES[ITEM_BUTCHERS_KNIFE].value = 5;
	PROTOTYPES[ITEM_BUTCHERS_KNIFE].weight = 2;
	PROTOTYPES[ITEM_BUTCHERS_KNIFE].min_dmg = 2;
	PROTOTYPES[ITEM_BUTCHERS_KNIFE].max_dmg = 8;
	PROTOTYPES[ITEM_BUTCHERS_KNIFE].attack_skill_use = SK_MELEE;
	PROTOTYPES[ITEM_BUTCHERS_KNIFE].heal_min = -2;
	PROTOTYPES[ITEM_BUTCHERS_KNIFE].heal_max = -6;
	PROTOTYPES[ITEM_BUTCHERS_KNIFE].name = "Butchers Knife";
	PROTOTYPES[ITEM_BUTCHERS_KNIFE].description = "A rusted butchers knife which does 2 - 8 damage.";

	PROTOTYPES[ITEM_LEATHER_JACKET].value = 12;
	PROTOTYPES[ITEM_LEATHER_JACKET].weight = 5;
	PROTOTYPES[ITEM_LEATHER_JACKET].matrix.skills[SK_ATTRACTION] = 10;
	PROTOTYPES[ITEM_LEATHER_JACKET].AC = 5;
	PROTOTYPES[ITEM_LEATHER_JACKET].name = "Leather Jacket";
	PROTOTYPES[ITEM_LEATHER_JACKET].description = "A Leather Jacket with 5 AC";

	PROTOTYPES[ITEM_POWER_ARMOR].value = 55;
	PROTOTYPES[ITEM_POWER_ARMOR].weight = 15;
	PROTOTYPES[ITEM_POWER_ARMOR].matrix.skills[SK_ATTRACTION] = 20;
	PROTOTYPES[ITEM_POWER_ARMOR].matrix.skills[SK_INTIMIDATE] = 20;
	PROTOTYPES[ITEM_POWER_ARMOR].AC = 15;
	PROTOTYPES[ITEM_POWER_ARMOR].name = "Power Armor";
	PROTOTYPES[ITEM_POWER_ARMOR].description = "Hulcking metal Power Armor with 15 AC";	
}

struct ItemPrototype *get_item_proto(enum ItemType type){
	return &PROTOTYPES[type];
};
