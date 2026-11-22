#include <raylib.h>
#include "util/gild_window.h"

#define FACTOR 1.1
#define MIN 0.5
#define MAX 6.0

void update_cam(Camera2D *cam){
	static float base_zoom = 1.0f;
	Vector2 cpos = GetMousePosition();

	// Scale the zoom when the window size changes
	if(IsWindowResized()){
		Vector2 world_pos_b = GetScreenToWorld2D(cpos, *cam);
		base_zoom = gild_wscale(base_zoom, GILD_BY_LARGEST);
		cam->zoom = base_zoom;
		Vector2 world_pos_a = GetScreenToWorld2D(cpos, *cam);

		// Center
		cam->target.x += world_pos_b.x - world_pos_a.x;
		cam->target.y += world_pos_b.y - world_pos_a.y;
	}

	// Do the input here eventually
	if(IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)){
		Vector2 delta = GetMouseDelta();
		cam->target.x -= delta.x / cam->zoom;
		cam->target.y -= delta.y / cam->zoom;
	}

	float wheel = GetMouseWheelMove();
	if(wheel != 0){
		Vector2 world_pos_b = GetScreenToWorld2D(cpos, *cam);
		float factor = FACTOR; 
		if(wheel > 0){base_zoom *= factor;}
		else{base_zoom /= factor;}

		// Clamp
		if(base_zoom < MIN){base_zoom = MAX;}
		if(base_zoom > MAX){base_zoom = MAX;}

		cam->zoom = base_zoom;
		Vector2 world_pos_a = GetScreenToWorld2D(cpos, *cam);
		// Center
		cam->target.x += world_pos_b.x - world_pos_a.x;
		cam->target.y += world_pos_b.y - world_pos_a.y;
	} else{cam->zoom = base_zoom;}
}
