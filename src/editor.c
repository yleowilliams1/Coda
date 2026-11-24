#include <raylib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/gild_mem.h"
#include "util/gild_basic.h"
#include "util/gild_log.h"
#include "util/gild_string.h"
#include "map.h"
#include "save.h"
#include "editor.h"
#include "camera.h"

#define RAYGUI_IMPLEMENTATION
#include "util/raygui.h"

// Controls
//   1 2 3 4 5   tool: floor, flags, ceiling, entity, transition
//   LMB         paint floor / set collide / drag a ceiling / place entity / drag a transition
//   RMB         floor back to 0 / clear collide / delete ceiling / delete entity / delete transition
//   Q / E       previous / next texture (skips ids with no file)
//   H           hide ceilings
//   Ctrl+S      save the .map and .ments
//   MMB, wheel  pan, zoom (camera.c)

#define PANEL_W 240
#define ROW 26
#define PROTO_MAX 128
#define UID_MAX 16
#define MAP_PATH_MAX 512
#define STATUS_SECONDS 4.0

enum Tool { TOOL_FLOOR, TOOL_FLAG, TOOL_CEILING, TOOL_ENTITY, TOOL_TRANSITION, TOOL_COUNT };
static const char *tool_names[TOOL_COUNT] = {"Floor", "Flags", "Ceiling", "Entity", "Transition"};
// Which panel text box has keyboard focus.
enum Focus { FOCUS_NONE, FOCUS_PROTO, FOCUS_UID, FOCUS_TPATH, FOCUS_TX, FOCUS_TY };
static const char *type_names[PROP + 1] = {"PLAYER", "NPC", "DOOR", "LOOT", "PROP"};

static Camera2D cam;
static struct Map m;
static char map_path[MAP_PATH_MAX];
static char *ent_path;
static bool dirty;

static enum Tool tool;
static u8 tex_id;
static bool hide_ceilings;

static s32 hover_tx, hover_ty;
static bool hover_in;

static bool dragging;
static enum Tool drag_tool;
static s32 drag_tx, drag_ty;

static enum Focus focus;

// Entity brush, set in the panel
static char proto_buf[PROTO_MAX];
static char uid_buf[UID_MAX];
static int ent_type = NPC;
static bool ent_collide = true;
static bool auto_uid;  // after placing, move the UID box to the next free uid

// Transition brush, set in the panel
static char tpath_buf[MAP_PATH_MAX];
static char ttx_buf[UID_MAX], tty_buf[UID_MAX];

static char status[160];
static double status_time = -100.0;

static void set_status(const char *fmt, ...){
	va_list args;
	va_start(args, fmt);
	vsnprintf(status, sizeof(status), fmt, args);
	va_end(args);
	status_time = GetTime();
}

static s32 clamp_s32(s32 v, s32 lo, s32 hi){return v < lo ? lo : (v > hi ? hi : v);}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

static void cycle_texture(int dir){
	for(int step = 1; step <= 256; step++){
		u8 id = (u8)(tex_id + dir * step);
		if(map_texture(id)){tex_id = id; return;}
	}
	set_status("No textures found in assets/textures");
}

static void add_ceiling(s32 tx0, s32 ty0, s32 tx1, s32 ty1){
	if(m.ceiling_count == UINT16_MAX){set_status("Too many ceilings"); return;}
	s32 x0 = tx0 < tx1 ? tx0 : tx1, x1 = tx0 < tx1 ? tx1 : tx0;
	s32 y0 = ty0 < ty1 ? ty0 : ty1, y1 = ty0 < ty1 ? ty1 : ty0;
	m.ceilings = GILD_XREALLOC(m.ceilings, ((size_t)m.ceiling_count + 1) * sizeof(struct MapCeiling));
	struct MapCeiling *c = &m.ceilings[m.ceiling_count++];
	c->x = (u16)x0;
	c->y = (u16)y0;
	c->w = (u16)(x1 - x0 + 1);
	c->h = (u16)(y1 - y0 + 1);
	c->sprite = tex_id;
	dirty = true;
}

// Deletes the most recently added ceiling covering the tile.
static void delete_ceiling_at(s32 tx, s32 ty){
	for(int i = (int)m.ceiling_count - 1; i >= 0; i--){
		const struct MapCeiling *c = &m.ceilings[i];
		if(tx < c->x || ty < c->y || tx >= (s32)c->x + c->w || ty >= (s32)c->y + c->h){continue;}
		memmove(&m.ceilings[i], &m.ceilings[i + 1], (size_t)(m.ceiling_count - i - 1) * sizeof(struct MapCeiling));
		m.ceiling_count--;
		if(m.ceiling_count == 0){free(m.ceilings); m.ceilings = NULL;}
		dirty = true;
		return;
	}
}

static bool parse_u32_text(const char *s, u32 *out){
	char *end;
	unsigned long v = strtoul(s, &end, 10);
	if(s[0] == '\0' || s[0] == '-' || *end != '\0' || v > UINT32_MAX){return false;}
	*out = (u32)v;
	return true;
}

static void add_transition(s32 tx0, s32 ty0, s32 tx1, s32 ty1){
	u32 dx, dy;
	if(tpath_buf[0] == '\0'){set_status("Set the target map path first"); return;}
	if(!parse_u32_text(ttx_buf, &dx) || !parse_u32_text(tty_buf, &dy)){set_status("Set the drop tile (two numbers) first"); return;}
	if(m.transition_count == UINT16_MAX){set_status("Too many transitions"); return;}
	s32 x0 = tx0 < tx1 ? tx0 : tx1, x1 = tx0 < tx1 ? tx1 : tx0;
	s32 y0 = ty0 < ty1 ? ty0 : ty1, y1 = ty0 < ty1 ? ty1 : ty0;
	m.transitions = GILD_XREALLOC(m.transitions, ((size_t)m.transition_count + 1) * sizeof(struct MapTransition));
	struct MapTransition *t = &m.transitions[m.transition_count++];
	t->x = (u16)x0;
	t->y = (u16)y0;
	t->w = (u16)(x1 - x0 + 1);
	t->h = (u16)(y1 - y0 + 1);
	t->path = gild_copy_alloc_str(tpath_buf, strlen(tpath_buf));
	t->target.x = dx;
	t->target.y = dy;
	dirty = true;
}

static bool transition_covers(const struct MapTransition *t, s32 tx, s32 ty){
	return tx >= t->x && ty >= t->y && tx < (s32)t->x + t->w && ty < (s32)t->y + t->h;
}

// Most recently added transition covering the tile, or -1.
static int transition_at(s32 tx, s32 ty){
	for(int i = (int)m.transition_count - 1; i >= 0; i--){
		if(transition_covers(&m.transitions[i], tx, ty)){return i;}
	}
	return -1;
}

static void delete_transition_at(s32 tx, s32 ty){
	int i = transition_at(tx, ty);
	if(i < 0){return;}
	free((void *)m.transitions[i].path);
	memmove(&m.transitions[i], &m.transitions[i + 1], (size_t)(m.transition_count - i - 1) * sizeof(struct MapTransition));
	m.transition_count--;
	if(m.transition_count == 0){free(m.transitions); m.transitions = NULL;}
	dirty = true;
}

static bool entity_on_tile(const struct Entity *e, s32 tx, s32 ty){
	s32 etx, ety;
	map_world_to_tile(&m, (float)e->pos.x, (float)e->pos.y, &etx, &ety);
	return etx == tx && ety == ty;
}

static bool uid_used(u32 uid){
	for(u16 i = 0; i < m.entities.count; i++){
		if(m.entities.items[i].uid == uid){return true;}
	}
	return false;
}

// Smallest unused uid >= start. False if every uid from start up to UINT32_MAX is taken.
static bool next_free_uid(u32 start, u32 *out){
	for(u32 v = start; ; v++){
		if(!uid_used(v)){*out = v; return true;}
		if(v == UINT32_MAX){return false;}
	}
}

static void add_entity(s32 tx, s32 ty){
	char *end;
	unsigned long typed = strtoul(uid_buf, &end, 10);
	if(uid_buf[0] == '\0' || *end != '\0' || typed > UINT32_MAX){set_status("Set a numeric UID first"); return;}
	u32 uid = (u32)typed;
	// update_entity moves the first match only, so a second entity with the same uid could never move.
	if(uid_used(uid)){
		if(!auto_uid){set_status("UID %u is already used", uid); return;}
		if(!next_free_uid(uid, &uid)){set_status("No free UID above %lu", typed); return;}
	}
	if(m.entities.count == UINT16_MAX){set_status("Too many entities"); return;}

	m.entities.items = GILD_XREALLOC(m.entities.items, ((size_t)m.entities.count + 1) * sizeof(struct Entity));
	struct Entity *e = &m.entities.items[m.entities.count++];
	// Stats, inventory, level and flags stay zeroed with initialised false: the game
	// fills them from the prototype the first time it loads the map.
	memset(e, 0, sizeof(*e));
	e->prototype = proto_buf[0] ? gild_copy_alloc_str(proto_buf, strlen(proto_buf)) : NULL;
	e->type = (enum MapEntityType)ent_type;
	e->uid = uid;
	e->pos = map_tile_to_world(&m, (u32)tx, (u32)ty);
	e->sprite = tex_id;
	e->should_collide = ent_collide ? 1 : 0;
	dirty = true;

	if(auto_uid){
		u32 next;
		if(uid < UINT32_MAX && next_free_uid(uid + 1, &next)){snprintf(uid_buf, sizeof(uid_buf), "%u", next);}
		if(uid != (u32)typed){set_status("UID %lu was taken, placed as %u", typed, uid);}
	}
}

// Deletes the front-most entity on the tile.
static void delete_entity_at(s32 tx, s32 ty){
	for(int i = (int)m.entities.count - 1; i >= 0; i--){
		if(!entity_on_tile(&m.entities.items[i], tx, ty)){continue;}
		free((void *)m.entities.items[i].prototype);
		memmove(&m.entities.items[i], &m.entities.items[i + 1], (size_t)(m.entities.count - i - 1) * sizeof(struct Entity));
		m.entities.count--;
		if(m.entities.count == 0){free(m.entities.items); m.entities.items = NULL;}
		dirty = true;
		return;
	}
}

static void save(void){
	if(write_map(&m, map_path) && write_map_entities(&m.entities, ent_path)){
		dirty = false;
		set_status("Saved %s", GetFileName(map_path));
	} else {
		set_status("Save FAILED, see the log");
	}
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

bool init_editor(const char *path, u16 w, u16 h){
	if(!path){GILD_LOG(GILD_IS_NULL, "Passed NULL map path"); return false;}
	if(strlen(path) >= sizeof(map_path)){GILD_LOG(GILD_INVALID_ARG, "Map path too long"); return false;}
	strcpy(map_path, path);
	ent_path = map_entity_path(map_path);
	if(!ent_path){return false;}

	if(w == 0 || h == 0){
		if(!read_map(&m, map_path) || !read_map_entities(&m.entities, ent_path)){
			free_map(&m);
			free(ent_path); ent_path = NULL;
			return false;
		}
	} else {
		// Creating over an existing map would wipe it on the first save.
		if(FileExists(map_path) || FileExists(ent_path)){
			GILD_LOG(GILD_DUPLICATE, "%s already exists. Open it with: editor %s", map_path, map_path);
			free(ent_path); ent_path = NULL;
			return false;
		}
		if((u32)w * h > MAP_MAX_TILES){
			GILD_LOG(GILD_INVALID_ARG, "Map %ux%u is over %u tiles", (u32)w, (u32)h, MAP_MAX_TILES);
			free(ent_path); ent_path = NULL;
			return false;
		}
		m.w = w;
		m.h = h;
		m.floors = GILD_XCALLOC((size_t)w * h, sizeof(u8));
		m.flags = GILD_XCALLOC((size_t)w * h, sizeof(u8));
		dirty = true;
	}

	// Centre the camera on the map.
	cam.zoom = 1.0f;
	cam.offset = (Vector2){(float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f};
	map_tile_to_worldf(&m, (m.w - 1) / 2.0f, (m.h - 1) / 2.0f, &cam.target.x, &cam.target.y);

	// Esc would close the window and lose unsaved work.
	SetExitKey(KEY_NULL);
	if(!map_texture(tex_id)){cycle_texture(1);}
	return true;
}

void update_editor(){
	update_cam(&cam);
	Vector2 mouse = GetMousePosition();
	Vector2 world = GetScreenToWorld2D(mouse, cam);
	hover_in = map_world_to_tile(&m, world.x, world.y, &hover_tx, &hover_ty);

	// While a text box has focus, keys are text, not shortcuts.
	bool typing = focus != FOCUS_NONE;
	// Clicking the map while a text box has focus ends the typing and still counts
	// as a click (otherwise the first click after typing a UID only closes the box).
	if(typing && mouse.x >= PANEL_W &&
	   (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))){
		focus = FOCUS_NONE;
		typing = false;
	}
	if(!typing){
		if(IsKeyPressed(KEY_ONE)){tool = TOOL_FLOOR;}
		if(IsKeyPressed(KEY_TWO)){tool = TOOL_FLAG;}
		if(IsKeyPressed(KEY_THREE)){tool = TOOL_CEILING;}
		if(IsKeyPressed(KEY_FOUR)){tool = TOOL_ENTITY;}
		if(IsKeyPressed(KEY_FIVE)){tool = TOOL_TRANSITION;}
		if(IsKeyPressed(KEY_Q)){cycle_texture(-1);}
		if(IsKeyPressed(KEY_E)){cycle_texture(1);}
		if(IsKeyPressed(KEY_H)){hide_ceilings = !hide_ceilings;}
		if((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_S)){save();}
	}
	if(tool != drag_tool){dragging = false;}

	// The panel and text boxes get the mouse first.
	if(typing || mouse.x < PANEL_W){
		if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){dragging = false;}
		return;
	}

	bool lmb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
	bool rmb = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
	u32 i = hover_in ? (u32)hover_ty * m.w + (u32)hover_tx : 0;

	switch(tool){
	case TOOL_FLOOR:
		if(!hover_in){break;}
		if(lmb && m.floors[i] != tex_id){m.floors[i] = tex_id; dirty = true;}
		else if(rmb && m.floors[i] != 0){m.floors[i] = 0; dirty = true;}
		break;
	case TOOL_FLAG:
		if(!hover_in){break;}
		if(lmb && !(m.flags[i] & MAP_FLAG_COLLIDE)){m.flags[i] |= MAP_FLAG_COLLIDE; dirty = true;}
		else if(rmb && (m.flags[i] & MAP_FLAG_COLLIDE)){m.flags[i] &= (u8)~MAP_FLAG_COLLIDE; dirty = true;}
		break;
	case TOOL_CEILING:
	case TOOL_TRANSITION:
		// Both are rectangles dragged out with LMB.
		if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover_in){
			dragging = true;
			drag_tool = tool;
			drag_tx = hover_tx;
			drag_ty = hover_ty;
		}
		if(dragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){
			dragging = false;
			s32 ex = clamp_s32(hover_tx, 0, (s32)m.w - 1), ey = clamp_s32(hover_ty, 0, (s32)m.h - 1);
			if(tool == TOOL_CEILING){add_ceiling(drag_tx, drag_ty, ex, ey);}
			else {add_transition(drag_tx, drag_ty, ex, ey);}
		}
		if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hover_in){
			if(tool == TOOL_CEILING){delete_ceiling_at(hover_tx, hover_ty);}
			else {delete_transition_at(hover_tx, hover_ty);}
		}
		break;
	case TOOL_ENTITY:
		if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && hover_in){add_entity(hover_tx, hover_ty);}
		if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hover_in){delete_entity_at(hover_tx, hover_ty);}
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static Vector2 iso(float tx, float ty){
	Vector2 v;
	map_tile_to_worldf(&m, tx, ty, &v.x, &v.y);
	return v;
}

// Outline of the tile rectangle [x0, x1] x [y0, y1] (inclusive tile coords).
static void outline_tiles(s32 x0, s32 y0, s32 x1, s32 y1, Color col){
	Vector2 a = iso(x0 - 0.5f, y0 - 0.5f), b = iso(x1 + 0.5f, y0 - 0.5f);
	Vector2 c = iso(x1 + 0.5f, y1 + 0.5f), d = iso(x0 - 0.5f, y1 + 0.5f);
	DrawLineV(a, b, col);
	DrawLineV(b, c, col);
	DrawLineV(c, d, col);
	DrawLineV(d, a, col);
}

static void fill_tile(s32 tx, s32 ty, Color col){
	Vector2 top = iso(tx - 0.5f, ty - 0.5f), right = iso(tx + 0.5f, ty - 0.5f);
	Vector2 bottom = iso(tx + 0.5f, ty + 0.5f), left = iso(tx - 0.5f, ty + 0.5f);
	DrawTriangle(top, left, bottom, col);
	DrawTriangle(top, bottom, right, col);
}

static void draw_overlays(void){
	// Grid: w+1 + h+1 straight lines.
	Color grid = Fade(LIGHTGRAY, 0.25f);
	for(s32 k = 0; k <= (s32)m.w; k++){DrawLineV(iso(k - 0.5f, -0.5f), iso(k - 0.5f, m.h - 0.5f), grid);}
	for(s32 k = 0; k <= (s32)m.h; k++){DrawLineV(iso(-0.5f, k - 0.5f), iso(m.w - 0.5f, k - 0.5f), grid);}

	if(tool == TOOL_FLAG){
		for(s32 ty = 0; ty < (s32)m.h; ty++){
			for(s32 tx = 0; tx < (s32)m.w; tx++){
				if(m.flags[(u32)ty * m.w + (u32)tx] & MAP_FLAG_COLLIDE){fill_tile(tx, ty, Fade(RED, 0.4f));}
			}
		}
	}
	if(tool == TOOL_CEILING){
		for(u16 i = 0; i < m.ceiling_count; i++){
			const struct MapCeiling *c = &m.ceilings[i];
			outline_tiles(c->x, c->y, c->x + c->w - 1, c->y + c->h - 1, SKYBLUE);
		}
	}
	if(tool == TOOL_TRANSITION){
		for(u16 i = 0; i < m.transition_count; i++){
			const struct MapTransition *t = &m.transitions[i];
			outline_tiles(t->x, t->y, t->x + t->w - 1, t->y + t->h - 1, MAGENTA);
		}
	}
	if(dragging){
		outline_tiles(drag_tx, drag_ty,
		              clamp_s32(hover_tx, 0, (s32)m.w - 1), clamp_s32(hover_ty, 0, (s32)m.h - 1), YELLOW);
	}
	if(tool == TOOL_ENTITY){
		// Entities whose texture is missing would otherwise be invisible.
		for(u16 i = 0; i < m.entities.count; i++){
			s32 tx, ty;
			map_world_to_tile(&m, (float)m.entities.items[i].pos.x, (float)m.entities.items[i].pos.y, &tx, &ty);
			outline_tiles(tx, ty, tx, ty, GREEN);
		}
	}
	if(hover_in){outline_tiles(hover_tx, hover_ty, hover_tx, hover_ty, WHITE);}
}

// A text box that takes focus when clicked and gives it up on Enter or a click elsewhere.
static void text_box(Rectangle r, char *buf, int size, enum Focus id){
	if(GuiTextBox(r, buf, size, focus == id)){focus = (focus == id) ? FOCUS_NONE : id;}
}

// raygui is immediate mode, so the panel's buttons are handled here as they are drawn.
// Only Button/Label/TextBox are used: their signatures are the same in raygui 3.x and 4.x.
static void draw_panel(void){
	DrawRectangle(0, 0, PANEL_W, GetScreenHeight(), GetColor((unsigned int)GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
	float x = 10, w = PANEL_W - 20, y = 10;

	GuiLabel((Rectangle){x, y, w, 20}, TextFormat("%s%s  (%ux%u)", GetFileName(map_path), dirty ? " *" : "", (u32)m.w, (u32)m.h));
	y += ROW;

	float half = (w - 6) / 2;
	for(int t = 0; t < TOOL_COUNT; t++){
		Rectangle r = {x + (t % 2) * (half + 6), y + (t / 2) * ROW, half, 22};
		if(GuiButton(r, TextFormat("%d %s", t + 1, tool_names[t]))){tool = (enum Tool)t;}
		if((int)tool == t){DrawRectangleLinesEx((Rectangle){r.x - 2, r.y - 2, r.width + 4, r.height + 4}, 2, ORANGE);}
	}
	y += ((TOOL_COUNT + 1) / 2) * ROW + 4;

	if(GuiButton((Rectangle){x, y, 30, 22}, "<")){cycle_texture(-1);}
	GuiLabel((Rectangle){x + 40, y, w - 80, 22}, TextFormat("Texture %d  (Q/E)", (int)tex_id));
	if(GuiButton((Rectangle){x + w - 30, y, 30, 22}, ">")){cycle_texture(1);}
	y += ROW;
	const struct Texture *t = map_texture(tex_id);
	if(t){DrawTexture(*t, (int)x, (int)y, WHITE);}
	else {GuiLabel((Rectangle){x, y, w, 20}, "(missing)");}
	y += 96 + 8;

	if(tool == TOOL_ENTITY){
		GuiLabel((Rectangle){x, y, w, 20}, "Prototype");
		y += 20;
		text_box((Rectangle){x, y, w, 22}, proto_buf, PROTO_MAX, FOCUS_PROTO);
		y += ROW;
		GuiLabel((Rectangle){x, y, w, 20}, uid_buf[0] ? "UID" : "UID  (needed to place)");
		y += 20;
		text_box((Rectangle){x, y, w, 22}, uid_buf, UID_MAX, FOCUS_UID);
		y += ROW;
		if(GuiButton((Rectangle){x, y, w, 22}, TextFormat("Type: %s", type_names[ent_type]))){ent_type = (ent_type + 1) % (PROP + 1);}
		y += ROW;
		if(GuiButton((Rectangle){x, y, w, 22}, auto_uid ? "Auto UID: on" : "Auto UID: off")){auto_uid = !auto_uid;}
		y += ROW;
		if(GuiButton((Rectangle){x, y, w, 22}, ent_collide ? "Collide: yes" : "Collide: no")){ent_collide = !ent_collide;}
		y += ROW + 4;
	}

	if(tool == TOOL_TRANSITION){
		GuiLabel((Rectangle){x, y, w, 20}, "Target map");
		y += 20;
		text_box((Rectangle){x, y, w, 22}, tpath_buf, MAP_PATH_MAX, FOCUS_TPATH);
		y += ROW;
		GuiLabel((Rectangle){x, y, w, 20}, "Drop at tile (x, y)");
		y += 20;
		text_box((Rectangle){x, y, half, 22}, ttx_buf, UID_MAX, FOCUS_TX);
		text_box((Rectangle){x + half + 6, y, half, 22}, tty_buf, UID_MAX, FOCUS_TY);
		y += ROW;
		// What the transition under the cursor does.
		int ti = hover_in ? transition_at(hover_tx, hover_ty) : -1;
		if(ti >= 0){
			const struct MapTransition *tr = &m.transitions[ti];
			GuiLabel((Rectangle){x, y, w, 20}, TextFormat("-> %s", tr->path));
			y += 18;
			GuiLabel((Rectangle){x, y, w, 20}, TextFormat("   at %u, %u", tr->target.x, tr->target.y));
			y += 18;
		}
		y += 4;
	}

	if(GuiButton((Rectangle){x, y, w, 22}, hide_ceilings ? "Ceilings: hidden (H)" : "Ceilings: shown (H)")){hide_ceilings = !hide_ceilings;}
	y += ROW;
	if(GuiButton((Rectangle){x, y, w, 22}, "Save (Ctrl+S)")){save();}
	y += ROW + 4;

	GuiLabel((Rectangle){x, y, w, 20}, hover_in ? TextFormat("Tile %d, %d", hover_tx, hover_ty) : "Tile -");
	y += 20;
	GuiLabel((Rectangle){x, y, w, 20}, TextFormat("%u entities, %u ceilings, %u exits",
	         (u32)m.entities.count, (u32)m.ceiling_count, (u32)m.transition_count));
	y += ROW;
	GuiLabel((Rectangle){x, y, w, 20}, "LMB place   RMB erase");
	y += 18;
	GuiLabel((Rectangle){x, y, w, 20}, "MMB pan   wheel zoom");
	y += 18;

	if(GetTime() - status_time < STATUS_SECONDS){
		GuiLabel((Rectangle){x, (float)GetScreenHeight() - 30, w, 20}, status);
	}
}

void draw_editor(){
	BeginDrawing();
	ClearBackground(BLACK);
	BeginMode2D(cam);
	// A copy, so hiding ceilings doesn't touch the real map. It shares the entity
	// array, which draw_map sorts in place (fine: nothing here keeps indices).
	struct Map view = m;
	if(hide_ceilings){view.ceiling_count = 0;}
	draw_map(&view);
	draw_overlays();
	EndMode2D();
	draw_panel();
	EndDrawing();
}

void free_editor(){
	free_map(&m);
	free(ent_path);
	ent_path = NULL;
}
