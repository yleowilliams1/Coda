#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "gild_basic.h"
#include "gild_log.h"
#include "gild_astar.h"
#include "gild_math.h"
#include "gild_mem.h"

// TODO: Fix this shit it sucks ass 
// This is bad but it works so it'll have to do

#define D_ORTHO 10
#define D_DIAG  14
#define NO_PARENT UINT32_MAX

static const s8 NB_DX[8] = { -1,  1, -1,  1,  0, -1,  0,  1 };
static const s8 NB_DY[8] = { -1, -1,  1,  1, -1,  0,  1,  0 };
static const s32 NB_COST[8] = { D_DIAG, D_DIAG, D_DIAG, D_DIAG, D_ORTHO, D_ORTHO, D_ORTHO, D_ORTHO };

static bool in_bounds_u32(u32 w, u32 h, u32 x, u32 y);
static bool in_bounds_signed(u32 w, u32 h, int x, int y);
static bool collide(u8 v, u8 mask) { return (v & mask) != 0; }
static u32 v2_idx(u32 x, u32 y, u32 w);

struct Heap {
	u32 *data;
	u32 count;
	u32 *pos;
	s32 *f_score;
	s32 *g_score;
};

static bool heap_less(struct Heap *h, u32 a, u32 b);
static void heap_swap(struct Heap *h, u32 i, u32 j);
static void heap_sift_up(struct Heap *h, u32 i);
static void heap_sift_down(struct Heap *h, u32 i);
static void heap_push(struct Heap *h, u32 node);
static void heap_decrease(struct Heap *h, u32 node);
static u32 heap_pop(struct Heap *h);

static inline int32_t heuristic(int dx, int dy) {
	dx = dx < 0 ? -dx : dx;
	dy = dy < 0 ? -dy : dy;
	int32_t mn = dx < dy ? dx : dy;
	int32_t mx = dx < dy ? dy : dx;
	return D_DIAG * mn + D_ORTHO * (mx - mn);
}

bool gild_pathfind(
		u8 *m, u32 w, u32 h,
		gild_v2_u32 start, gild_v2_u32 target,
		struct Gild_AstarPath *out) {
	return gild_pathfind_mask(m, w, h, start, target, 1u, out);
}

bool gild_pathfind_mask(
		u8 *m, u32 w, u32 h,
		gild_v2_u32 start, gild_v2_u32 target, u8 block_mask,
		struct Gild_AstarPath *out) {
	if (!m) {
		GILD_LOG(GILD_IS_NULL, "Map is NULL");
		return false;
	}
	if (w == 0 || h == 0) {
		return false;
	}
	if (!in_bounds_u32(w, h, start.x, start.y) || !in_bounds_u32(w, h, target.x, target.y)) {
		GILD_LOG(GILD_OUT_OF_BOUNDS, "Start or goal is out of map bounds");
		return false;
	}
	if (collide(m[v2_idx(target.x, target.y, w)], block_mask)) {
		return false;
	}
	if (start.x == target.x && start.y == target.y) {
		return false;
	}

	u32 tile_count = w * h;
	s32 *g_score = GILD_XCALLOC(tile_count, sizeof(s32));
	s32 *f_score = GILD_XCALLOC(tile_count, sizeof(s32));
	u32 *parent = GILD_XCALLOC(tile_count, sizeof(u32));
	u8 *state = GILD_XCALLOC(tile_count, sizeof(u8));

	struct Heap heap = {
		.data = GILD_XCALLOC(tile_count, sizeof(u32)),
		.count = 0,
		.pos = GILD_XCALLOC(tile_count, sizeof(u32)),
		.f_score = f_score,
		.g_score = g_score,
	};

	memset(parent, 0xFF, tile_count * sizeof(u32));
	memset(heap.pos, 0xFF, tile_count * sizeof(u32));

	u32 s = v2_idx(start.x, start.y, w);
	u32 g = v2_idx(target.x, target.y, w);

	state[s] = 1;
	heap_push(&heap, s);

	bool found = false;
	while (heap.count > 0) {
		u32 current = heap_pop(&heap);
		if (current == g) {
			found = true;
			break;
		}
		state[current] = 2;

		int cx = (int)(current % w);
		int cy = (int)(current / w);

		for (int dir = 0; dir < 8; dir++) {
			int nx = cx + NB_DX[dir];
			int ny = cy + NB_DY[dir];
			if (!in_bounds_signed(w, h, nx, ny)) {
				continue;
			}

			u32 unx = (u32)nx;
			u32 uny = (u32)ny;
			u8 tile = m[v2_idx(unx, uny, w)];
			if (collide(tile, block_mask)) {
				continue;
			}

			// Don't let diagonal moves cut across a blocked corner
			if (NB_DX[dir] != 0 && NB_DY[dir] != 0) {
				u32 adj1_x = (u32)(cx + NB_DX[dir]);
				u32 adj1_y = (u32)cy;
				u32 adj2_x = (u32)cx;
				u32 adj2_y = (u32)(cy + NB_DY[dir]);

				if (collide(m[v2_idx(adj1_x, adj1_y, w)], block_mask) || collide(m[v2_idx(adj2_x, adj2_y, w)], block_mask)) {
					continue;
				}
			}

			u32 neighbor = v2_idx(unx, uny, w);
			if (state[neighbor] == 2) {
				continue;
			}

			s32 tentative_g = g_score[current] + NB_COST[dir];
			if (state[neighbor] == 0 || tentative_g < g_score[neighbor]) {
				parent[neighbor] = current;
				g_score[neighbor] = tentative_g;
				f_score[neighbor] = tentative_g + heuristic((int)target.x - nx, (int)target.y - ny);
				if (state[neighbor] == 1) {
					heap_decrease(&heap, neighbor);
				} else {
					state[neighbor] = 1;
					heap_push(&heap, neighbor);
				}
			}
		}
	}

	if (found) {
		u32 len = 1;
		for (u32 n = g; n != s; n = parent[n]) {
			len++;
		}

		gild_v2_u32 *coords = GILD_XCALLOC(len, sizeof(gild_v2_u32));
		u32 n = g;
		for (u32 i = len; i-- > 0;) {
			coords[i].x = n % w;
			coords[i].y = n / w;
			if (n == s) {
				break;
			}
			n = parent[n];
		}

		if (out) {
			out->path = coords;
			out->path_length = len;
		} else {
			free(coords);
		}
	}

	free(g_score);
	free(f_score);
	free(parent);
	free(state);
	free(heap.data);
	free(heap.pos);

	return found;
}

static bool in_bounds_u32(u32 w, u32 h, u32 x, u32 y) {
	return x < w && y < h;
}

static bool in_bounds_signed(u32 w, u32 h, int x, int y) {
	return x >= 0 && y >= 0 && (u32)x < w && (u32)y < h;
}

static u32 v2_idx(u32 x, u32 y, u32 w) {
	return y * w + x;
}

static bool heap_less(struct Heap *h, u32 a, u32 b) {
	if (h->f_score[a] != h->f_score[b]) {
		return h->f_score[a] < h->f_score[b];
	}
	return h->g_score[a] > h->g_score[b];
}

static void heap_swap(struct Heap *h, u32 i, u32 j) {
	u32 tmp = h->data[i];
	h->data[i] = h->data[j];
	h->data[j] = tmp;
	h->pos[h->data[i]] = i;
	h->pos[h->data[j]] = j;
}

static void heap_sift_up(struct Heap *h, u32 i) {
	while (i > 0) {
		u32 parent = (i - 1) / 2;
		if (!heap_less(h, h->data[i], h->data[parent])) {
			break;
		}
		heap_swap(h, i, parent);
		i = parent;
	}
}

static void heap_sift_down(struct Heap *h, u32 i) {
	for (;;) {
		u32 left = 2 * i + 1;
		u32 right = 2 * i + 2;
		u32 smallest = i;
		if (left < h->count && heap_less(h, h->data[left], h->data[smallest])) {
			smallest = left;
		}
		if (right < h->count && heap_less(h, h->data[right], h->data[smallest])) {
			smallest = right;
		}
		if (smallest == i) {
			break;
		}
		heap_swap(h, i, smallest);
		i = smallest;
	}
}

static void heap_push(struct Heap *h, u32 node) {
	u32 i = h->count++;
	h->data[i] = node;
	h->pos[node] = i;
	heap_sift_up(h, i);
}

static void heap_decrease(struct Heap *h, u32 node) {
	heap_sift_up(h, h->pos[node]);
}

static u32 heap_pop(struct Heap *h) {
	u32 top = h->data[0];
	h->count--;
	if (h->count > 0) {
		h->data[0] = h->data[h->count];
		h->pos[h->data[0]] = 0;
		heap_sift_down(h, 0);
	}
	h->pos[top] = NO_PARENT;
	return top;
}
