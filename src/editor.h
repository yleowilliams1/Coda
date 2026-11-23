#pragma once
#include "util/gild_basic.h"

// Opens the map at path (and the .ments file next to it) when w or h is 0,
// otherwise creates a new w x h map there (refuses if the file already exists).
// Call after InitWindow.
bool init_editor(const char *path, u16 w, u16 h);
void update_editor();
void draw_editor();
void free_editor();
