#pragma once
#include "gild_basic.h"

struct Gild_Rng{
	u64 state;
	u64 inc;
};

s32 gild_rng(s32 lo, s32 hi);
