#include "gild_basic.h"
#include "gild_rand.h"

static struct Gild_Rng r = {0};

u32 gild_rng_next(){
	u64 old = r.state;
	r.state = old * 6364136223846793005ULL + r.inc;
	u32 xorshifted = (u32)(((old >> 18u) ^ old) >> 27u);
	u32 rot = (u32)(old >> 59u);
	return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
}

s32 gild_rng(s32 lo, s32 hi){
	u32 span = (u32)hi - (u32)lo + 1u;
	if(span == 0){return (s32)gild_rng_next();}
	u32 threshold = (0u - span) % span;
	for(;;){
		u32 x = gild_rng_next();
		if(x >= threshold){return (s32)((u32)lo + x % span);}
	}
}
