#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "util/gild_math.h"
#include "util/gild_basic.h"
#include "map.h"

#define NOT_FOUND 0
#define FOUND 1
#define NO_PLAYER 2

static Texture2D tex[256];
static int compare(const void *a, const void *b){
	struct MapEntity *x = (struct MapEntity *)a;
	struct MapEntity *y = (struct MapEntity *)b;

	if(x->pos.y - 80 < y->pos.y - 80){return -1;}
	if(x->pos.y - 80 > y->pos.y - 80){return 1;}
	return 0;
}
void update_entity(struct Map *m, u32 uid, gild_v2_u32 new_pos, u8 new_sprite){
	if(!m || !m->entities){return;}
	for(u32 i = 0; i < m->entity_count; i++){
		if(m->entities[i].uid != uid){continue;}
		m->entities[i].pos = new_pos;
		m->entities[i].sprite = new_sprite;
		return;
	}
	return;
}
void draw_map(struct Map *m, bool sort) {
	if (!m || !m->floors || !m->entities) { return; }
	if(m->player_state == NOT_FOUND){
		int found = 0;
		for(int i = 0; i < m->entity_count; i++){
			if(m->entities[i].type == PLAYER){
				m->player_state = FOUND;
				m->player = i;
				found = 1;
				return;
			}	
		}
		if(!found){m->player_state = NO_PLAYER;}
	}
	const int tile_w = 64;
	const int tile_h = 96;
	const int origin_x = 32; // center of 64px width
	const int origin_y = 80; // 16px up from bottom of 96px sprite (96 - 16)

	for (u32 i = 0; i < (m->w * m->h); i++) {
		u32 floor_id = m->floors[i];
		if (tex[floor_id].id == 0) {
			char buff[256];
			snprintf(buff, sizeof(buff), "assets/textures/%d.png", (int)floor_id);
			tex[floor_id] = LoadTexture(buff);
		}

		int world_x = (int)((i % m->w) * (u32)tile_w);
		int world_y = (int)((i / m->w) * (u32)tile_h);
		DrawTexture(tex[floor_id], world_x - origin_x, world_y - origin_y, WHITE);
	}

	if (sort) {
		qsort(m->entities, m->entity_count, sizeof(struct MapEntity), compare);
	}

	for (u32 i = 0; i < m->entity_count; i++) {
		u32 spr_id = m->entities[i].sprite;
		if (tex[spr_id].id == 0) {
			char buff[256];
			snprintf(buff, sizeof(buff), "assets/textures/%d.png", (int)spr_id);
			tex[spr_id] = LoadTexture(buff);
		}

		int ent_x = (int)m->entities[i].pos.x;
		int ent_y = (int)m->entities[i].pos.y;
		DrawTexture(tex[spr_id], ent_x - origin_x, ent_y - origin_y, WHITE);
	}

	int has_player = (m->player_state == FOUND && m->player < m->entity_count);
	u16 px = 0, py = 0;
	if (has_player) {
		px = (u16)(m->entities[m->player].pos.x / (u32)tile_w);
		py = (u16)(m->entities[m->player].pos.y / (u32)tile_h);
	}
	for (u16 i = 0; i < m->ceiling_count; i++) {
		const struct MapCeiling *c = &m->ceilings[i];
		if (has_player &&
		    px >= c->x && px < (u32)c->x + (u32)c->w &&
		    py >= c->y && py < (u32)c->y + (u32)c->h) {
			continue; 
		}
		if(tex[c->sprite].id == 0){
			char buff[256];
			snprintf(buff, sizeof(buff), "assets/textures/%d.png", (int)c->sprite);
			tex[c->sprite] = LoadTexture(buff);		
		}
		for (u16 ty = c->y; ty < c->y + c->h; ty++) {
			for (u16 tx = c->x; tx < c->x + c->w; tx++) {
				int world_x = (int)((u32)tx * (u32)tile_w);
				int world_y = (int)((u32)ty * (u32)tile_h);
				DrawTexture(tex[c->sprite], world_x - origin_x, world_y - origin_y, WHITE);			
			}
		}
	}
}
