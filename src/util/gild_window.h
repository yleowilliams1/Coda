#pragma once
#include "gild_basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_WINDOW_W 1280
#define DEFAULT_WINDOW_H 720

#define GILD_WIN_PARAM(X) \
	X(GILD_BY_X) \
	X(GILD_BY_Y) \
	X(GILD_BY_LARGEST) \
	X(GILD_BY_SMALLEST)

#define GILD_WIN_PARAM_ENTRY_(id) id,
enum Gild_WindowScaleParameter{
	GILD_WIN_PARAM(GILD_WIN_PARAM_ENTRY_)
};
#undef GILD_WIN_PARAM_ENTRY_

struct Gild_WindowScaler{
	u32 w;
	u32 h;
	float scale_x;
	float scale_y;
};

// Call gild_init_wscaler once with the size the UI was designed for; until the
// first gild_update_wscaler the scale is 1 (gild_wscale returns num unchanged).
GILD_API void  gild_init_wscaler(int default_w, int default_h);
GILD_API void  gild_update_wscaler(int new_w, int new_h);
GILD_API float gild_wscale(float num, enum Gild_WindowScaleParameter param);

#ifdef __cplusplus
}
#endif
