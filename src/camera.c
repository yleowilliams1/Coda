#include <raylib.h>
#include "util/gild_window.h"

#define ZOOM_FACTOR 1.1f
#define ZOOM_MIN    0.5f
#define ZOOM_MAX    6.0f

// Two separate numbers:
//   user_zoom  what the mouse wheel sets, clamped to [ZOOM_MIN, ZOOM_MAX]
//   scale      how much bigger the window is than DEFAULT_WINDOW_W x H
// cam->zoom is always user_zoom * scale, worked out fresh each frame. The old code
// multiplied the stored zoom by the window scale on every resize event, so the
// scale got applied again and again (and skipped the clamp), drifting further
// with each resize.
//
// Call gild_update_wscaler before this on a resize frame (main.c does).
void update_cam(Camera2D *cam){
	static float user_zoom = 1.0f;
	static int last_w = 0, last_h = 0;

	int sw = GetScreenWidth(), sh = GetScreenHeight();
	if(last_w == 0 || last_h == 0){last_w = sw; last_h = sh;}

	// A resize moves the offset by the same fraction of the screen (the centre stays
	// the centre). target is the world point under the offset, so what was in the
	// middle of the screen stays there. A minimised window reports 0 x 0: skip it.
	if(sw > 0 && sh > 0 && (sw != last_w || sh != last_h)){
		cam->offset.x *= (float)sw / (float)last_w;
		cam->offset.y *= (float)sh / (float)last_h;
		last_w = sw;
		last_h = sh;
	}

	float scale = gild_wscale(1.0f, GILD_BY_LARGEST);

	if(IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)){
		Vector2 delta = GetMouseDelta();
		cam->target.x -= delta.x / cam->zoom;
		cam->target.y -= delta.y / cam->zoom;
	}

	float wheel = GetMouseWheelMove();
	if(wheel != 0){
		// Zoom towards the cursor: the world point under it stays under it.
		Vector2 cpos = GetMousePosition();
		Vector2 world_before = GetScreenToWorld2D(cpos, *cam);

		if(wheel > 0){user_zoom *= ZOOM_FACTOR;}
		else {user_zoom /= ZOOM_FACTOR;}
		if(user_zoom < ZOOM_MIN){user_zoom = ZOOM_MIN;}
		if(user_zoom > ZOOM_MAX){user_zoom = ZOOM_MAX;}

		cam->zoom = user_zoom * scale;
		Vector2 world_after = GetScreenToWorld2D(cpos, *cam);
		cam->target.x += world_before.x - world_after.x;
		cam->target.y += world_before.y - world_after.y;
	}
	cam->zoom = user_zoom * scale;
}
