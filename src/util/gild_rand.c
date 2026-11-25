#include "gild_basic.h"
#include "gild_rand.h"

// PCG32's default state and increment (pcg_basic.h, PCG32_INITIALIZER). The old
// {0, 0} start was a fixed point: state stayed 0, every output was 0, and the
// rejection loop in gild_rng never ended.
static struct Gild_Rng r = {0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL};

// Same steps as pcg32_srandom_r.
void gild_rng_seed(u64 seed, u64 seq){
	r.state = 0u;
	r.inc = (seq << 1u) | 1u; // odd, or the generator's period collapses
	gild_rng_next();
	r.state += seed;
	gild_rng_next();
}

u32 gild_rng_next(void){
	u64 old = r.state;
	r.state = old * 6364136223846793005ULL + r.inc;
	u32 xorshifted = (u32)(((old >> 18u) ^ old) >> 27u);
	u32 rot = (u32)(old >> 59u);
	return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
}

s32 gild_rng(s32 lo, s32 hi){
	if(lo > hi){s32 t = lo; lo = hi; hi = t;}
	u32 span = (u32)hi - (u32)lo + 1u;
	if(span == 0){return (s32)gild_rng_next();} // the full s32 range
	// Rejection sampling: drop the few values that would make x % span uneven.
	u32 threshold = (0u - span) % span;
	for(;;){
		u32 x = gild_rng_next();
		if(x >= threshold){return (s32)((u32)lo + x % span);}
	}
}
