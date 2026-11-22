#include <raylib.h>
#include <string.h>
#include <unistd.h>
#include "util/gild_window.h"

#ifdef _POSIX_VERSION
#include "cli.h"
#endif

int main(int argc, char *argv[]){
	#ifdef _POSIX_VERSION
	if(argc > 1 && strcmp(argv[1], "cli") == 0){
		if(!run_cli()){
			return 0;
		}
	}
	#endif
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
