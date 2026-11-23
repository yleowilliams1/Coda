#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "util/gild_math.h"
#include "util/gild_basic.h"
#include "util/gild_log.h"
#include "map.h"

#define NOT_FOUND 0
#define FOUND 1
#define NO_PLAYER 2

static Texture2D tex[256];
static bool tex_tried[256];
static bool tex_warned[256];

const struct Texture *map_texture(u8 id){
	// Only try each file once: retrying a missing texture every frame floods the log.
	if(!tex_tried[id]){
		tex_tried[id] = true;
		char buff[64];
		snprintf(buff, sizeof(buff), MAP_TEXTURE_PATH, (int)id);
		if(FileExists(buff)){tex[id] = LoadTexture(buff);}
	}
	return tex[id].id != 0 ? &tex[id] : NULL;
}

// Draws texture `id` with the shared origin on the anchor (ax, ay).
static void draw_tex(u8 id, s32 ax, s32 ay){
	const struct Texture *t = map_texture(id);
	if(!t){
		if(!tex_warned[id]){
			tex_warned[id] = true;
			GILD_LOG(GILD_NOT_FOUND, "Missing texture " MAP_TEXTURE_PATH, (int)id);
		}
		return;
	}
	DrawTexture(*t, ax - TEX_ORIGIN_X, ay - TEX_ORIGIN_Y, WHITE);
}

gild_v2_u32 map_tile_to_world(const struct Map *m, u32 tx, u32 ty){
	gild_v2_u32 r;
	r.x = gild_safe_cast(u32, ((s64)tx - (s64)ty + (s64)m->h) * (TILE_W / 2));
	r.y = gild_safe_cast(u32, ((s64)tx + (s64)ty + 1) * (TILE_H / 2));
	return r;
}

void map_tile_to_worldf(const struct Map *m, float tx, float ty, float *x, float *y){
	*x = (tx - ty + (float)m->h) * (TILE_W / 2.0f);
	*y = (tx + ty + 1.0f) * (TILE_H / 2.0f);
}

bool map_world_to_tile(const struct Map *m, float x, float y, s32 *tx, s32 *ty){
	// Undo the projection: a = tx - ty, b = tx + ty. Rounding (rather than flooring)
	// because the anchor is the diamond's centre.
	float a = x / (TILE_W / 2.0f) - (float)m->h;
	float b = y / (TILE_H / 2.0f) - 1.0f;
	*tx = (s32)floorf((a + b) * 0.5f + 0.5f);
	*ty = (s32)floorf((b - a) * 0.5f + 0.5f);
	return *tx >= 0 && *ty >= 0 && *tx < (s32)m->w && *ty < (s32)m->h;
}

// Back to front: lower y first, then left to right.
static int compare(const void *a, const void *b){
	const struct MapEntity *x = (const struct MapEntity *)a;
	const struct MapEntity *y = (const struct MapEntity *)b;
	if(x->pos.y != y->pos.y){return x->pos.y < y->pos.y ? -1 : 1;}
	if(x->pos.x != y->pos.x){return x->pos.x < y->pos.x ? -1 : 1;}
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

void draw_map(struct Map *m){
	if(!m || !m->floors || m->w == 0 || m->h == 0){return;}
	u16 ecount = m->entities ? m->entity_count : 0;

	if(ecount > 1){qsort(m->entities, ecount, sizeof(struct MapEntity), compare);}

	// Found again every frame, after the sort, because the sort moves the player around.
	m->player_state = NO_PLAYER;
	for(u16 i = 0; i < ecount; i++){
		if(m->entities[i].type == PLAYER){
			m->player_state = FOUND;
			m->player = i;
			break;
		}
	}

	// Floors one diagonal (tx + ty = d) at a time, back to front. After each diagonal,
	// draw the entities that stand on it (anchor y below the next diagonal's), so the
	// walls of nearer tiles are drawn over them.
	u16 e = 0;
	u32 diagonals = (u32)m->w + (u32)m->h - 1;
	for(u32 d = 0; d < diagonals; d++){
		u32 tx_min = d >= m->h ? d - ((u32)m->h - 1) : 0;
		u32 tx_max = d < m->w ? d : (u32)m->w - 1;
		for(u32 tx = tx_min; tx <= tx_max; tx++){
			u32 ty = d - tx;
			gild_v2_u32 a = map_tile_to_world(m, tx, ty);
			draw_tex(m->floors[ty * m->w + tx], (s32)a.x, (s32)a.y);
		}
		u32 next_y = (d + 2) * (TILE_H / 2);
		while(e < ecount && m->entities[e].pos.y < next_y){
			const struct MapEntity *ent = &m->entities[e++];
			draw_tex(ent->sprite, (s32)ent->pos.x, (s32)ent->pos.y);
		}
	}
	// Anything below the map's last diagonal.
	while(e < ecount){
		const struct MapEntity *ent = &m->entities[e++];
		draw_tex(ent->sprite, (s32)ent->pos.x, (s32)ent->pos.y);
	}

	// Ceilings over everything, except the one the player is standing under.
	s32 ptx = -1, pty = -1;
	if(m->player_state == FOUND){
		const struct MapEntity *p = &m->entities[m->player];
		map_world_to_tile(m, (float)p->pos.x, (float)p->pos.y, &ptx, &pty);
	}
	for(u16 i = 0; i < m->ceiling_count && m->ceilings; i++){
		const struct MapCeiling *c = &m->ceilings[i];
		u32 x_end = (u32)c->x + c->w, y_end = (u32)c->y + c->h;
		if(ptx >= (s32)c->x && ptx < (s32)x_end && pty >= (s32)c->y && pty < (s32)y_end){continue;}
		if(x_end > m->w){x_end = m->w;}
		if(y_end > m->h){y_end = m->h;}
		// Row by row is already back to front for every pair of ceiling tiles that overlap on screen.
		for(u32 ty = c->y; ty < y_end; ty++){
			for(u32 tx = c->x; tx < x_end; tx++){
				gild_v2_u32 a = map_tile_to_world(m, tx, ty);
				draw_tex(c->sprite, (s32)a.x, (s32)a.y);
			}
		}
	}
}
