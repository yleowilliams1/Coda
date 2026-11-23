#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include "util/gild_basic.h"
#include "util/gild_window.h"
#include "util/gild_log.h"
#include "map.h"
#include "editor.h"
#ifdef _POSIX_VERSION
#include "cli.h"
#endif

static bool parse_dim(const char *s, u16 *out){
	char *end;
	long v = strtol(s, &end, 10);
	if(*s == '\0' || *end != '\0' || v < 1 || v > UINT16_MAX){return false;}
	*out = (u16)v;
	return true;
}

int main(int argc, char *argv[]){
	#ifdef _POSIX_VERSION
	if(argc > 1 && strcmp(argv[1], "cli") == 0){
		if(!run_cli()){
			return 0;
		}
	}
	#endif

	// editor <map path>            open a map (and the .ments file next to it)
	// editor <map path> <w> <h>    create a new w x h map at that path
	// Paths are relative to the executable's folder, like assets/.
	bool editor = false;
	const char *map_path = NULL;
	u16 new_w = 0, new_h = 0;
	if(argc > 1 && strcmp(argv[1], "editor") == 0){
		bool ok = false;
		if(argc == 3){
			map_path = argv[2];
			ok = true;
		} else if(argc == 5){
			map_path = argv[2];
			ok = parse_dim(argv[3], &new_w) && parse_dim(argv[4], &new_h) &&
			     (u32)new_w * new_h <= MAP_MAX_TILES;
		}
		if(!ok){
			fprintf(stderr, "usage: %s editor <map path> [<w> <h>]\n"
			                "  w and h from 1 to 65535, at most %u tiles in total\n", argv[0], MAP_MAX_TILES);
			return 1;
		}
		editor = true;
	}
	ChangeDirectory(GetApplicationDirectory());
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);

	gild_init_wscaler(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H);
	InitWindow(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H, "Coda");

	if(editor && !init_editor(map_path, new_w, new_h)){
		CloseWindow();
		return 1;
	}
	while(!WindowShouldClose()){
		if(IsWindowResized()){
			gild_update_wscaler(GetScreenWidth(), GetScreenHeight());
		}

		if(editor){
			update_editor();
			draw_editor();
		} else {
			// Nothing to draw yet, but without Begin/EndDrawing the window never
			// processes events and can't be closed.
			BeginDrawing();
			ClearBackground(BLACK);
			EndDrawing();
		}
	}

	CloseWindow();
	if(editor){free_editor();}
	return 0;
}
