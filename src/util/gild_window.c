#include "gild_window.h"
#include "gild_basic.h"
#include "gild_log.h"

static struct Gild_WindowScaler w = {0, 0, 1.0f, 1.0f};

void gild_init_wscaler(int default_w, int default_h){
	if(default_w <= 0 || default_h <= 0){
		GILD_LOG(GILD_INVALID_ARG, "Window size must be positive (got %d x %d)", default_w, default_h);
		return;
	}
	w.w = (u32)default_w;
	w.h = (u32)default_h;
	w.scale_x = 1.0f;
	w.scale_y = 1.0f;
}
void gild_update_wscaler(int new_w, int new_h){
	if(w.w == 0 || w.h == 0){
		GILD_LOG(GILD_INVALID_STATE, "gild_update_wscaler called before gild_init_wscaler");
		return;
	}
	// A minimised window reports 0 x 0 on some platforms (e.g. Windows).
	// Keep the previous scale rather than collapsing everything to 0.
	if(new_w <= 0 || new_h <= 0){return;}

	w.scale_x = (float)new_w / (float)w.w;
	w.scale_y = (float)new_h / (float)w.h;
}
float gild_wscale(float num, enum Gild_WindowScaleParameter param){
	float result = num;

	float x = w.scale_x;
	float y = w.scale_y;

	switch(param){
		case GILD_BY_X:
			result *= x;
			break;
		case GILD_BY_Y:
			result *= y;
			break;
		case GILD_BY_LARGEST:
			result *= (x > y) ? x : y;
			break;
		case GILD_BY_SMALLEST:
			result *= (x < y) ? x : y;
			break;
		default:
			break;
	}
	return result;
}
