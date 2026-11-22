#include <raylib.h>
#include "util/gild_window.h"

int main(void){
	ChangeDirectory(GetApplicationDirectory());
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	
	gild_init_wscaler(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H);
	InitWindow(DEFAULT_WINDOW_W, DEFAULT_WINDOW_H, "Coda");
	while(!WindowShouldClose()){
		if(IsWindowResized()){
			gild_update_wscaler(GetScreenWidth(), GetScreenHeight());
		}		
	}

	CloseWindow();
}
