#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "util/gild_basic.h"
#include "util/gild_window.h"
#include "util/gild_log.h"
#include "util/gild_rand.h"

int main(int argc, char *argv[]){
	// Different dice every run. Use a fixed number here to make runs repeatable.
	gild_rng_seed((u64)time(NULL), 0);
	ChangeDirectory(GetApplicationDirectory());
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);

	gild_init_wscaler(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H);
	InitWindow(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H, "Coda");

	while(!WindowShouldClose()){
		if(IsWindowResized()){
			gild_update_wscaler(GetScreenWidth(), GetScreenHeight());
		}

		BeginDrawing();
		ClearBackground(BLACK);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
