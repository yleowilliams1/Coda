#pragma once
#include "gild_basic.h"

// PCG32 (pcg-random.org). One global generator.
//
// It starts from PCG's published default state, so it works (and gives the same
// sequence every run) without seeding. Call gild_rng_seed once at startup for a
// different sequence each run, e.g. gild_rng_seed((u64)time(NULL), 0).
// Seeding with a fixed value makes runs repeatable (handy for tests and replays).

struct Gild_Rng{
	u64 state;
	u64 inc; // must be odd; gild_rng_seed makes sure of that
};

// seq picks one of 2^63 independent streams; 0 is fine if you only need one.
void gild_rng_seed(u64 seed, u64 seq);

// Next raw 32-bit value.
u32 gild_rng_next(void);

// Uniform in [lo, hi], both inclusive. lo > hi is swapped.
s32 gild_rng(s32 lo, s32 hi);
