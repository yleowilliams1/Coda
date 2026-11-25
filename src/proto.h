#pragma once
#include "util/gild_basic.h"

void load_item_proto(void);
void load_character_proto(void);

const struct CharacterPrototype *get_character_proto(u32 id);
const struct ItemPrototype *get_item_proto(u32 id);
