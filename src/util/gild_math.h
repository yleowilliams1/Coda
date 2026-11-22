#pragma once
#include "gild_basic.h"

#ifdef __cplusplus
extern "C" {
#endif

// Functions available (V = vector type, T = its component type):
//
//   V    gild_V_add(V a, V b)       all types
//   V    gild_V_sub(V a, V b)       all types
//   V    gild_V_mult(V a, V b)      all types
//   V    gild_V_div(V a, V b)       all types (zero divisor -> 0)
//   V    gild_V_mults(V a, T s)     all types
//   V    gild_V_divs(V a, T s)      all types (zero divisor -> 0)
//   V    gild_V_cross(V a, V b)     3D types only (v3, vf3, v3_s8 .. v3_u64)
//   V    gild_V_norm(V a)           vf2, vf3 only
//   bool gild_V_is_norm(V a)        vf2, vf3 only
//
//   bool gild_R_collide_point(R r, P p)   rect types only; P = matching v2 type
//   bool gild_R_collide_aabb(R a, R b)    rect types only; touching edges do not collide
//                                         (rects are half-open: [x, x+w) x [y, y+h))
//
// All arithmetic is overflow-checked via gild_basic.h: on overflow a message is
// logged via GILD_LOG() and the wrapped result is returned. Division by zero
// is logged and gives 0 for that component.
//
// Types (all prefixed gild_): v2 v3 vf2 vf3 v2_s8 v2_u8 v2_s16 v2_u16 v2_s32 v2_u32
//        v2_s64 v2_u64 v3_s8 v3_u8 v3_s16 v3_u16 v3_s32 v3_u32 v3_s64 v3_u64
//        rec recf rec_s8 rec_u8 rec_s16 rec_u16 rec_s32 rec_u32 rec_s64 rec_u64
//          (rect types have x, y, w, h and support add/sub/mult/div/mults/divs only)
// Example: gild_vf3 a; gild_vf3_add(a, b); gild_v2_u8_mults(p, 2); gild_v3_s16_cross(a, b)

// Generic
// gild_v2/gild_v3 are int-based (s32); gild_vf2/gild_vf3 are float-based.
typedef struct gild_v2{
    union { struct { s32 x, y; }; s32 e[2]; };
}gild_v2;
GILD_STATIC_ASSERT(sizeof(gild_v2) == 2 * sizeof(s32), "gild_v2 has unexpected padding");
typedef struct gild_vf2{
    union { struct { float x, y; }; float e[2]; };
}gild_vf2;
GILD_STATIC_ASSERT(sizeof(gild_vf2) == 2 * sizeof(float), "gild_vf2 has unexpected padding");

typedef struct gild_v3{
    union { struct { s32 x, y, z; }; s32 e[3]; };
}gild_v3;
GILD_STATIC_ASSERT(sizeof(gild_v3) == 3 * sizeof(s32), "gild_v3 has unexpected padding");
typedef struct gild_vf3{
    union { struct { float x, y, z; }; float e[3]; };
}gild_vf3;
GILD_STATIC_ASSERT(sizeof(gild_vf3) == 3 * sizeof(float), "gild_vf3 has unexpected padding");

// Typed 8s
typedef struct gild_v2_s8{
    union { struct { s8 x, y; }; s8 e[2]; };
}gild_v2_s8;
GILD_STATIC_ASSERT(sizeof(gild_v2_s8) == 2 * sizeof(s8), "gild_v2_s8 has unexpected padding");
typedef struct gild_v3_s8{
    union { struct { s8 x, y, z; }; s8 e[3]; };
}gild_v3_s8;
GILD_STATIC_ASSERT(sizeof(gild_v3_s8) == 3 * sizeof(s8), "gild_v3_s8 has unexpected padding");

typedef struct gild_v2_u8{
    union { struct { u8 x, y; }; u8 e[2]; };
}gild_v2_u8;
GILD_STATIC_ASSERT(sizeof(gild_v2_u8) == 2 * sizeof(u8), "gild_v2_u8 has unexpected padding");
typedef struct gild_v3_u8{
    union { struct { u8 x, y, z; }; u8 e[3]; };
}gild_v3_u8;
GILD_STATIC_ASSERT(sizeof(gild_v3_u8) == 3 * sizeof(u8), "gild_v3_u8 has unexpected padding");

// Typed 16s
typedef struct gild_v2_s16{
    union { struct { s16 x, y; }; s16 e[2]; };
}gild_v2_s16;
GILD_STATIC_ASSERT(sizeof(gild_v2_s16) == 2 * sizeof(s16), "gild_v2_s16 has unexpected padding");
typedef struct gild_v3_s16{
    union { struct { s16 x, y, z; }; s16 e[3]; };
}gild_v3_s16;
GILD_STATIC_ASSERT(sizeof(gild_v3_s16) == 3 * sizeof(s16), "gild_v3_s16 has unexpected padding");

typedef struct gild_v2_u16{
    union { struct { u16 x, y; }; u16 e[2]; };
}gild_v2_u16;
GILD_STATIC_ASSERT(sizeof(gild_v2_u16) == 2 * sizeof(u16), "gild_v2_u16 has unexpected padding");
typedef struct gild_v3_u16{
    union { struct { u16 x, y, z; }; u16 e[3]; };
}gild_v3_u16;
GILD_STATIC_ASSERT(sizeof(gild_v3_u16) == 3 * sizeof(u16), "gild_v3_u16 has unexpected padding");

// Typed 32s
typedef struct gild_v2_s32{
    union { struct { s32 x, y; }; s32 e[2]; };
}gild_v2_s32;
GILD_STATIC_ASSERT(sizeof(gild_v2_s32) == 2 * sizeof(s32), "gild_v2_s32 has unexpected padding");
typedef struct gild_v3_s32{
    union { struct { s32 x, y, z; }; s32 e[3]; };
}gild_v3_s32;
GILD_STATIC_ASSERT(sizeof(gild_v3_s32) == 3 * sizeof(s32), "gild_v3_s32 has unexpected padding");

typedef struct gild_v2_u32{
    union { struct { u32 x, y; }; u32 e[2]; };
}gild_v2_u32;
GILD_STATIC_ASSERT(sizeof(gild_v2_u32) == 2 * sizeof(u32), "gild_v2_u32 has unexpected padding");
typedef struct gild_v3_u32{
    union { struct { u32 x, y, z; }; u32 e[3]; };
}gild_v3_u32;
GILD_STATIC_ASSERT(sizeof(gild_v3_u32) == 3 * sizeof(u32), "gild_v3_u32 has unexpected padding");

// Typed 64s
typedef struct gild_v2_s64{
    union { struct { s64 x, y; }; s64 e[2]; };
}gild_v2_s64;
GILD_STATIC_ASSERT(sizeof(gild_v2_s64) == 2 * sizeof(s64), "gild_v2_s64 has unexpected padding");
typedef struct gild_v3_s64{
    union { struct { s64 x, y, z; }; s64 e[3]; };
}gild_v3_s64;
GILD_STATIC_ASSERT(sizeof(gild_v3_s64) == 3 * sizeof(s64), "gild_v3_s64 has unexpected padding");

typedef struct gild_v2_u64{
    union { struct { u64 x, y; }; u64 e[2]; };
}gild_v2_u64;
GILD_STATIC_ASSERT(sizeof(gild_v2_u64) == 2 * sizeof(u64), "gild_v2_u64 has unexpected padding");
typedef struct gild_v3_u64{
    union { struct { u64 x, y, z; }; u64 e[3]; };
}gild_v3_u64;
GILD_STATIC_ASSERT(sizeof(gild_v3_u64) == 3 * sizeof(u64), "gild_v3_u64 has unexpected padding");

// Rectangles: x, y, w, h
// gild_rec is int-based (s32); gild_recf is float-based.
typedef struct gild_rec{
    union { struct { s32 x, y, w, h; }; s32 e[4]; };
}gild_rec;
GILD_STATIC_ASSERT(sizeof(gild_rec) == 4 * sizeof(s32), "gild_rec has unexpected padding");
typedef struct gild_recf{
    union { struct { float x, y, w, h; }; float e[4]; };
}gild_recf;
GILD_STATIC_ASSERT(sizeof(gild_recf) == 4 * sizeof(float), "gild_recf has unexpected padding");


// Typed rectangles
typedef struct gild_rec_s8{
    union { struct { s8 x, y, w, h; }; s8 e[4]; };
}gild_rec_s8;
GILD_STATIC_ASSERT(sizeof(gild_rec_s8) == 4 * sizeof(s8), "gild_rec_s8 has unexpected padding");
typedef struct gild_rec_u8{
    union { struct { u8 x, y, w, h; }; u8 e[4]; };
}gild_rec_u8;
GILD_STATIC_ASSERT(sizeof(gild_rec_u8) == 4 * sizeof(u8), "gild_rec_u8 has unexpected padding");
typedef struct gild_rec_s16{
    union { struct { s16 x, y, w, h; }; s16 e[4]; };
}gild_rec_s16;
GILD_STATIC_ASSERT(sizeof(gild_rec_s16) == 4 * sizeof(s16), "gild_rec_s16 has unexpected padding");
typedef struct gild_rec_u16{
    union { struct { u16 x, y, w, h; }; u16 e[4]; };
}gild_rec_u16;
GILD_STATIC_ASSERT(sizeof(gild_rec_u16) == 4 * sizeof(u16), "gild_rec_u16 has unexpected padding");
typedef struct gild_rec_s32{
    union { struct { s32 x, y, w, h; }; s32 e[4]; };
}gild_rec_s32;
GILD_STATIC_ASSERT(sizeof(gild_rec_s32) == 4 * sizeof(s32), "gild_rec_s32 has unexpected padding");
typedef struct gild_rec_u32{
    union { struct { u32 x, y, w, h; }; u32 e[4]; };
}gild_rec_u32;
GILD_STATIC_ASSERT(sizeof(gild_rec_u32) == 4 * sizeof(u32), "gild_rec_u32 has unexpected padding");
typedef struct gild_rec_s64{
    union { struct { s64 x, y, w, h; }; s64 e[4]; };
}gild_rec_s64;
GILD_STATIC_ASSERT(sizeof(gild_rec_s64) == 4 * sizeof(s64), "gild_rec_s64 has unexpected padding");
typedef struct gild_rec_u64{
    union { struct { u64 x, y, w, h; }; u64 e[4]; };
}gild_rec_u64;
GILD_STATIC_ASSERT(sizeof(gild_rec_u64) == 4 * sizeof(u64), "gild_rec_u64 has unexpected padding");

#define GILD_VEC_DECL_COMMON(V, T) \
    GILD_API V V##_add(V a, V b); \
    GILD_API V V##_sub(V a, V b); \
    GILD_API V V##_mult(V a, V b); \
    GILD_API V V##_div(V a, V b); \
    GILD_API V V##_mults(V a, T s); \
    GILD_API V V##_divs(V a, T s);
#define GILD_REC_DECL_COLLIDE(R, P) \
    GILD_API bool R##_collide_point(R r, P p); \
    GILD_API bool R##_collide_aabb(R a, R b);
#define GILD_VEC_DECL_CROSS(V) GILD_API V V##_cross(V a, V b);
#define GILD_VEC_DECL_NORM(V)  GILD_API V V##_norm(V a); GILD_API bool V##_is_norm(V a);
 
GILD_VEC_DECL_COMMON(gild_v2,  s32)
GILD_VEC_DECL_COMMON(gild_vf2, float)
GILD_VEC_DECL_COMMON(gild_v3,  s32)
GILD_VEC_DECL_COMMON(gild_vf3, float)
GILD_VEC_DECL_NORM(gild_vf2)
GILD_VEC_DECL_NORM(gild_vf3)
GILD_VEC_DECL_CROSS(gild_v3)
GILD_VEC_DECL_CROSS(gild_vf3)
 
GILD_VEC_DECL_COMMON(gild_v2_s8,  s8)
GILD_VEC_DECL_COMMON(gild_v2_u8,  u8)
GILD_VEC_DECL_COMMON(gild_v2_s16, s16)
GILD_VEC_DECL_COMMON(gild_v2_u16, u16)
GILD_VEC_DECL_COMMON(gild_v2_s32, s32)
GILD_VEC_DECL_COMMON(gild_v2_u32, u32)
GILD_VEC_DECL_COMMON(gild_v2_s64, s64)
GILD_VEC_DECL_COMMON(gild_v2_u64, u64)
 
GILD_VEC_DECL_COMMON(gild_v3_s8,  s8)
GILD_VEC_DECL_COMMON(gild_v3_u8,  u8)
GILD_VEC_DECL_COMMON(gild_v3_s16, s16)
GILD_VEC_DECL_COMMON(gild_v3_u16, u16)
GILD_VEC_DECL_COMMON(gild_v3_s32, s32)
GILD_VEC_DECL_COMMON(gild_v3_u32, u32)
GILD_VEC_DECL_COMMON(gild_v3_s64, s64)
GILD_VEC_DECL_COMMON(gild_v3_u64, u64)
 
GILD_VEC_DECL_CROSS(gild_v3_s8)
GILD_VEC_DECL_CROSS(gild_v3_u8)
GILD_VEC_DECL_CROSS(gild_v3_s16)
GILD_VEC_DECL_CROSS(gild_v3_u16)
GILD_VEC_DECL_CROSS(gild_v3_s32)
GILD_VEC_DECL_CROSS(gild_v3_u32)
GILD_VEC_DECL_CROSS(gild_v3_s64)
GILD_VEC_DECL_CROSS(gild_v3_u64)

// Rectangles (component-wise ops only)
GILD_VEC_DECL_COMMON(gild_rec, s32)
GILD_VEC_DECL_COMMON(gild_recf, float)
GILD_VEC_DECL_COMMON(gild_rec_s8, s8)
GILD_VEC_DECL_COMMON(gild_rec_u8, u8)
GILD_VEC_DECL_COMMON(gild_rec_s16, s16)
GILD_VEC_DECL_COMMON(gild_rec_u16, u16)
GILD_VEC_DECL_COMMON(gild_rec_s32, s32)
GILD_VEC_DECL_COMMON(gild_rec_u32, u32)
GILD_VEC_DECL_COMMON(gild_rec_s64, s64)
GILD_VEC_DECL_COMMON(gild_rec_u64, u64)

// Rectangle collision (point type P = matching gild_v2 type)
GILD_REC_DECL_COLLIDE(gild_rec, gild_v2)
GILD_REC_DECL_COLLIDE(gild_recf, gild_vf2)
GILD_REC_DECL_COLLIDE(gild_rec_s8, gild_v2_s8)
GILD_REC_DECL_COLLIDE(gild_rec_u8, gild_v2_u8)
GILD_REC_DECL_COLLIDE(gild_rec_s16, gild_v2_s16)
GILD_REC_DECL_COLLIDE(gild_rec_u16, gild_v2_u16)
GILD_REC_DECL_COLLIDE(gild_rec_s32, gild_v2_s32)
GILD_REC_DECL_COLLIDE(gild_rec_u32, gild_v2_u32)
GILD_REC_DECL_COLLIDE(gild_rec_s64, gild_v2_s64)
GILD_REC_DECL_COLLIDE(gild_rec_u64, gild_v2_u64)

#ifdef __cplusplus
}
#endif
