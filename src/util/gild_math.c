// See gild_math.h for the list of functions and types.

#include <math.h>
#include "gild_math.h"
#include "gild_basic.h"

#define VEC2_DEF(V, T) \
V V##_add(V a, V b){ V r; r.x = gild_safe_add(a.x, b.x); r.y = gild_safe_add(a.y, b.y); return r; } \
V V##_sub(V a, V b){ V r; r.x = gild_safe_sub(a.x, b.x); r.y = gild_safe_sub(a.y, b.y); return r; } \
V V##_mult(V a, V b){ V r; r.x = gild_safe_mult(a.x, b.x); r.y = gild_safe_mult(a.y, b.y); return r; } \
V V##_div(V a, V b){ V r; r.x = gild_safe_div(a.x, b.x); r.y = gild_safe_div(a.y, b.y); return r; } \
V V##_mults(V a, T s){ V r; r.x = gild_safe_mult(a.x, s); r.y = gild_safe_mult(a.y, s); return r; } \
V V##_divs(V a, T s){ V r; r.x = gild_safe_div(a.x, s); r.y = gild_safe_div(a.y, s); return r; }

#define VEC3_DEF(V, T) \
V V##_add(V a, V b){ V r; r.x = gild_safe_add(a.x, b.x); r.y = gild_safe_add(a.y, b.y); r.z = gild_safe_add(a.z, b.z); return r; } \
V V##_sub(V a, V b){ V r; r.x = gild_safe_sub(a.x, b.x); r.y = gild_safe_sub(a.y, b.y); r.z = gild_safe_sub(a.z, b.z); return r; } \
V V##_mult(V a, V b){ V r; r.x = gild_safe_mult(a.x, b.x); r.y = gild_safe_mult(a.y, b.y); r.z = gild_safe_mult(a.z, b.z); return r; } \
V V##_div(V a, V b){ V r; r.x = gild_safe_div(a.x, b.x); r.y = gild_safe_div(a.y, b.y); r.z = gild_safe_div(a.z, b.z); return r; } \
V V##_mults(V a, T s){ V r; r.x = gild_safe_mult(a.x, s); r.y = gild_safe_mult(a.y, s); r.z = gild_safe_mult(a.z, s); return r; } \
V V##_divs(V a, T s){ V r; r.x = gild_safe_div(a.x, s); r.y = gild_safe_div(a.y, s); r.z = gild_safe_div(a.z, s); return r; }

#define REC_DEF(V, T) \
V V##_add(V a, V b){ V r; r.x = gild_safe_add(a.x, b.x); r.y = gild_safe_add(a.y, b.y); r.w = gild_safe_add(a.w, b.w); r.h = gild_safe_add(a.h, b.h); return r; } \
V V##_sub(V a, V b){ V r; r.x = gild_safe_sub(a.x, b.x); r.y = gild_safe_sub(a.y, b.y); r.w = gild_safe_sub(a.w, b.w); r.h = gild_safe_sub(a.h, b.h); return r; } \
V V##_mult(V a, V b){ V r; r.x = gild_safe_mult(a.x, b.x); r.y = gild_safe_mult(a.y, b.y); r.w = gild_safe_mult(a.w, b.w); r.h = gild_safe_mult(a.h, b.h); return r; } \
V V##_div(V a, V b){ V r; r.x = gild_safe_div(a.x, b.x); r.y = gild_safe_div(a.y, b.y); r.w = gild_safe_div(a.w, b.w); r.h = gild_safe_div(a.h, b.h); return r; } \
V V##_mults(V a, T s){ V r; r.x = gild_safe_mult(a.x, s); r.y = gild_safe_mult(a.y, s); r.w = gild_safe_mult(a.w, s); r.h = gild_safe_mult(a.h, s); return r; } \
V V##_divs(V a, T s){ V r; r.x = gild_safe_div(a.x, s); r.y = gild_safe_div(a.y, s); r.w = gild_safe_div(a.w, s); r.h = gild_safe_div(a.h, s); return r; }

// Collision. Rects are half-open: [x, x+w) x [y, y+h), so touching edges do not
// collide and a rect with w or h of 0 collides with nothing.
// Written with subtraction instead of x+w so that x+w can't overflow for any
// type, and so that unsigned types never wrap (the larger value is always on
// the left of the minus).
// KNOWN ISSUE: for the signed 32/64-bit types (rec, rec_s32, rec_s64 and
// their points) the subtraction itself can still overflow, which is UB, when
// the two coordinates are more than INT_MAX apart, e.g. x = INT_MIN vs
// x = INT_MAX. s8/s16 are safe (promoted to int) and floats are safe.
#define IN_RANGE(p, x, w)        ((p) >= (x) && ((p) - (x)) < (w))
#define AXIS_OVERLAP(ax, aw, bx, bw) \
	((aw) > 0 && (bw) > 0 && \
	 ((ax) < (bx) ? ((bx) - (ax)) < (aw) : ((ax) - (bx)) < (bw)))

#define REC_COLLIDE_DEF(R, P) \
bool R##_collide_point(R r, P p){ \
	return (bool)(IN_RANGE(p.x, r.x, r.w) && IN_RANGE(p.y, r.y, r.h)); \
} \
bool R##_collide_aabb(R a, R b){ \
	return (bool)(AXIS_OVERLAP(a.x, a.w, b.x, b.w) && AXIS_OVERLAP(a.y, a.h, b.y, b.h)); \
}

#define VEC3_CROSS_DEF(V, T) \
V V##_cross(V a, V b){ \
	V r; \
	r.x = gild_safe_sub(gild_safe_mult(a.y, b.z), gild_safe_mult(a.z, b.y)); \
	r.y = gild_safe_sub(gild_safe_mult(a.z, b.x), gild_safe_mult(a.x, b.z)); \
	r.z = gild_safe_sub(gild_safe_mult(a.x, b.y), gild_safe_mult(a.y, b.x)); \
	return r; \
}

// Normalizing the zero vector returns the zero vector.
#define VEC2F_NORM_DEF(V) \
V V##_norm(V a){ \
	float l2  = gild_safe_add(gild_safe_mult(a.x, a.x), gild_safe_mult(a.y, a.y)); \
	float inv = l2 > 0.0f ? gild_safe_div(1.0f, sqrtf(l2)) : 0.0f; \
	V r; r.x = gild_safe_mult(a.x, inv); r.y = gild_safe_mult(a.y, inv); \
	return r; \
} \
bool V##_is_norm(V a){ \
	float l2 = gild_safe_add(gild_safe_mult(a.x, a.x), gild_safe_mult(a.y, a.y)); \
	return fabsf(gild_safe_sub(l2, 1.0f)) <= VF_NORM_EPSILON; \
}

#define VEC3F_NORM_DEF(V) \
V V##_norm(V a){ \
	float l2  = gild_safe_add(gild_safe_add(gild_safe_mult(a.x, a.x), gild_safe_mult(a.y, a.y)), gild_safe_mult(a.z, a.z)); \
	float inv = l2 > 0.0f ? gild_safe_div(1.0f, sqrtf(l2)) : 0.0f; \
	V r; r.x = gild_safe_mult(a.x, inv); r.y = gild_safe_mult(a.y, inv); r.z = gild_safe_mult(a.z, inv); \
	return r; \
} \
bool V##_is_norm(V a){ \
	float l2 = gild_safe_add(gild_safe_add(gild_safe_mult(a.x, a.x), gild_safe_mult(a.y, a.y)), gild_safe_mult(a.z, a.z)); \
	return fabsf(gild_safe_sub(l2, 1.0f)) <= VF_NORM_EPSILON; \
}

// Tolerance on |v|^2 for is_norm.
#define VF_NORM_EPSILON 1e-4f

// Generic
VEC2_DEF(gild_v2,  s32)
VEC2_DEF(gild_vf2, float)
VEC3_DEF(gild_v3,  s32)
VEC3_DEF(gild_vf3, float)
VEC2F_NORM_DEF(gild_vf2)
VEC3F_NORM_DEF(gild_vf3)

VEC3_CROSS_DEF(gild_v3,  s32)
VEC3_CROSS_DEF(gild_vf3, float)

// Typed 2D
VEC2_DEF(gild_v2_s8,  s8)
VEC2_DEF(gild_v2_u8,  u8)
VEC2_DEF(gild_v2_s16, s16)
VEC2_DEF(gild_v2_u16, u16)
VEC2_DEF(gild_v2_s32, s32)
VEC2_DEF(gild_v2_u32, u32)
VEC2_DEF(gild_v2_s64, s64)
VEC2_DEF(gild_v2_u64, u64)

// Typed 3D
VEC3_DEF(gild_v3_s8,  s8)
VEC3_DEF(gild_v3_u8,  u8)
VEC3_DEF(gild_v3_s16, s16)
VEC3_DEF(gild_v3_u16, u16)
VEC3_DEF(gild_v3_s32, s32)
VEC3_DEF(gild_v3_u32, u32)
VEC3_DEF(gild_v3_s64, s64)
VEC3_DEF(gild_v3_u64, u64)

VEC3_CROSS_DEF(gild_v3_s8,  s8)
VEC3_CROSS_DEF(gild_v3_u8,  u8)
VEC3_CROSS_DEF(gild_v3_s16, s16)
VEC3_CROSS_DEF(gild_v3_u16, u16)
VEC3_CROSS_DEF(gild_v3_s32, s32)
VEC3_CROSS_DEF(gild_v3_u32, u32)
VEC3_CROSS_DEF(gild_v3_s64, s64)
VEC3_CROSS_DEF(gild_v3_u64, u64)

// Rectangles
REC_DEF(gild_rec, s32)
REC_DEF(gild_recf, float)
REC_DEF(gild_rec_s8, s8)
REC_DEF(gild_rec_u8, u8)
REC_DEF(gild_rec_s16, s16)
REC_DEF(gild_rec_u16, u16)
REC_DEF(gild_rec_s32, s32)
REC_DEF(gild_rec_u32, u32)
REC_DEF(gild_rec_s64, s64)
REC_DEF(gild_rec_u64, u64)

// Rectangle collision
REC_COLLIDE_DEF(gild_rec, gild_v2)
REC_COLLIDE_DEF(gild_recf, gild_vf2)
REC_COLLIDE_DEF(gild_rec_s8, gild_v2_s8)
REC_COLLIDE_DEF(gild_rec_u8, gild_v2_u8)
REC_COLLIDE_DEF(gild_rec_s16, gild_v2_s16)
REC_COLLIDE_DEF(gild_rec_u16, gild_v2_u16)
REC_COLLIDE_DEF(gild_rec_s32, gild_v2_s32)
REC_COLLIDE_DEF(gild_rec_u32, gild_v2_u32)
REC_COLLIDE_DEF(gild_rec_s64, gild_v2_s64)
REC_COLLIDE_DEF(gild_rec_u64, gild_v2_u64)
