#pragma once

// Rule is nothing other than this header file includes system headers

#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Export / import macro, put on every public function.                */
/*   Build the library as a DLL/.so : -DGILD_BUILD_DLL                 */
/*   Build/consume as a static lib  : -DGILD_STATIC                    */
/*   Consume the DLL                : (define nothing)                 */
/* The CMake build sets these for you (and for anything linking it).   */
/* ------------------------------------------------------------------ */
#if defined(_WIN32) || defined(__CYGWIN__)
	#if defined(GILD_STATIC)
		#define GILD_API
	#elif defined(GILD_BUILD_DLL)
		#define GILD_API __declspec(dllexport)
	#else
		#define GILD_API __declspec(dllimport)
	#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
	#define GILD_API __attribute__((visibility("default")))
#else
	#define GILD_API
#endif

// Compile-time assert usable from both C and C++ (C++ spells it static_assert).
#ifdef __cplusplus
	#define GILD_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
	#define GILD_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

typedef uint8_t u8;   typedef int8_t s8;
typedef uint16_t u16; typedef int16_t s16;
typedef uint32_t u32; typedef int32_t s32;
typedef uint64_t u64; typedef int64_t s64;

// ---------------------------------------------------------------------------
// Overflow-checked arithmetic:  gild_safe_add / _sub / _mult / _div (a, b)
// Dispatches on the type of `a`: s8..s64, u8..u64, float, and size_t.
// ---------------------------------------------------------------------------
#define GILD_SAFE_DECL(T, N) \
    GILD_API T gild_safe_add_##N(T a, T b, const char *file, s32 line, const char *func); \
    GILD_API T gild_safe_sub_##N(T a, T b, const char *file, s32 line, const char *func); \
    GILD_API T gild_safe_mult_##N(T a, T b, const char *file, s32 line, const char *func); \
    GILD_API T gild_safe_div_##N(T a, T b, const char *file, s32 line, const char *func);

GILD_SAFE_DECL(s8,     s8)
GILD_SAFE_DECL(u8,     u8)
GILD_SAFE_DECL(s16,    s16)
GILD_SAFE_DECL(u16,    u16)
GILD_SAFE_DECL(s32,    s32)
GILD_SAFE_DECL(u32,    u32)
GILD_SAFE_DECL(s64,    s64)
GILD_SAFE_DECL(u64,    u64)
GILD_SAFE_DECL(float,  f32)
GILD_SAFE_DECL(size_t, size)

// Anything that isn't a supported type lands here. Declared but never defined;
// with GCC/Clang the attribute turns a use into a readable compile error,
// elsewhere it is a link error.
#if defined(__GNUC__) || defined(__clang__)
__attribute__((error("gild_safe_*: operand type is not s8..s64, u8..u64, float or size_t")))
#endif
void gild_safe_unsupported_type(const void *a, ...);

// size_t is the same type as one of u32/u64 on Linux and 64-bit Windows (the
// main branches handle it there) but a distinct type on macOS and some others.
// Listing it in the main _Generic would be a duplicate-association error on
// the first kind of platform, so it is only checked in the fallback.
#define GILD_SAFE_SIZE_FALLBACK(op, a) _Generic((a), \
    size_t:  gild_safe_##op##_size, \
    default: gild_safe_unsupported_type)

#define GILD_SAFE_DISPATCH(op, a, b) _Generic((a), \
    s8:  gild_safe_##op##_s8,  u8:  gild_safe_##op##_u8, \
    s16: gild_safe_##op##_s16, u16: gild_safe_##op##_u16, \
    s32: gild_safe_##op##_s32, u32: gild_safe_##op##_u32, \
    s64: gild_safe_##op##_s64, u64: gild_safe_##op##_u64, \
    float: gild_safe_##op##_f32, \
    default: GILD_SAFE_SIZE_FALLBACK(op, a))((a), (b), __FILE__, __LINE__, __func__)

#define gild_safe_add(a, b)  GILD_SAFE_DISPATCH(add,  a, b)
#define gild_safe_sub(a, b)  GILD_SAFE_DISPATCH(sub,  a, b)
#define gild_safe_mult(a, b) GILD_SAFE_DISPATCH(mult, a, b)
#define gild_safe_div(a, b)  GILD_SAFE_DISPATCH(div,  a, b)

// ---------------------------------------------------------------------------
// Modulo (integers and size_t only; use fmodf yourself for floats).
// gild_safe_mod(a, b): b == 0 and MIN % -1 are logged, and return 0.
// ---------------------------------------------------------------------------
#define GILD_SAFE_MOD_DECL(T, N) \
    GILD_API T gild_safe_mod_##N(T a, T b, const char *file, s32 line, const char *func);

GILD_SAFE_MOD_DECL(s8,     s8)
GILD_SAFE_MOD_DECL(u8,     u8)
GILD_SAFE_MOD_DECL(s16,    s16)
GILD_SAFE_MOD_DECL(u16,    u16)
GILD_SAFE_MOD_DECL(s32,    s32)
GILD_SAFE_MOD_DECL(u32,    u32)
GILD_SAFE_MOD_DECL(s64,    s64)
GILD_SAFE_MOD_DECL(u64,    u64)
GILD_SAFE_MOD_DECL(size_t, size)

#define GILD_SAFE_DISPATCH_INT(op, a, b) _Generic((a), \
    s8:  gild_safe_##op##_s8,  u8:  gild_safe_##op##_u8, \
    s16: gild_safe_##op##_s16, u16: gild_safe_##op##_u16, \
    s32: gild_safe_##op##_s32, u32: gild_safe_##op##_u32, \
    s64: gild_safe_##op##_s64, u64: gild_safe_##op##_u64, \
    default: GILD_SAFE_SIZE_FALLBACK(op, a))((a), (b), __FILE__, __LINE__, __func__)

#define gild_safe_mod(a, b) GILD_SAFE_DISPATCH_INT(mod, a, b)

// ---------------------------------------------------------------------------
// Bit helpers: gild_bit_set(x, n), gild_bit_clear(x, n), gild_bit_check(x, n)
// ---------------------------------------------------------------------------
#define GILD_SAFE_BIT_DECL(T, N) \
    GILD_API void gild_safe_bit_set_##N(T *p, u32 n, const char *file, s32 line, const char *func); \
    GILD_API void gild_safe_bit_clear_##N(T *p, u32 n, const char *file, s32 line, const char *func); \
    GILD_API bool gild_safe_bit_check_##N(T x, u32 n, const char *file, s32 line, const char *func);

GILD_SAFE_BIT_DECL(s8,  s8)
GILD_SAFE_BIT_DECL(u8,  u8)
GILD_SAFE_BIT_DECL(s16, s16)
GILD_SAFE_BIT_DECL(u16, u16)
GILD_SAFE_BIT_DECL(s32, s32)
GILD_SAFE_BIT_DECL(u32, u32)
GILD_SAFE_BIT_DECL(s64, s64)
GILD_SAFE_BIT_DECL(u64, u64)

#define GILD_SAFE_BIT_PTR_DISPATCH(op, x) _Generic(&(x), \
    s8*:  gild_safe_bit_##op##_s8,  u8*:  gild_safe_bit_##op##_u8, \
    s16*: gild_safe_bit_##op##_s16, u16*: gild_safe_bit_##op##_u16, \
    s32*: gild_safe_bit_##op##_s32, u32*: gild_safe_bit_##op##_u32, \
    s64*: gild_safe_bit_##op##_s64, u64*: gild_safe_bit_##op##_u64)

#define gild_bit_set(x, n)   GILD_SAFE_BIT_PTR_DISPATCH(set,   x)(&(x), (n), __FILE__, __LINE__, __func__)
#define gild_bit_clear(x, n) GILD_SAFE_BIT_PTR_DISPATCH(clear, x)(&(x), (n), __FILE__, __LINE__, __func__)
#define gild_bit_check(x, n) _Generic((x), \
    s8:  gild_safe_bit_check_s8,  u8:  gild_safe_bit_check_u8, \
    s16: gild_safe_bit_check_s16, u16: gild_safe_bit_check_u16, \
    s32: gild_safe_bit_check_s32, u32: gild_safe_bit_check_u32, \
    s64: gild_safe_bit_check_s64, u64: gild_safe_bit_check_u64)((x), (n), __FILE__, __LINE__, __func__)

// ---------------------------------------------------------------------------
// Checked conversions between s8 u8 s16 u16 s32 u32 s64 u64 and size_t.
//
//   u8     a = gild_safe_cast(u8, some_u32);      // logs if the value doesn't fit
//   size_t n = gild_safe_cast(size_t, some_s32);  // logs if negative
//
// The destination is a type NAME (s8, u8, ... size_t), the source is detected
// from the expression's type. On failure the wrapped (C-cast) value is returned
// and GILD_OVERFLOW is logged, same as gild_safe_add/sub/mult.
// Source expressions must have one of the types above. Anything else (char,
// float, double, bool, ...) is rejected at compile time. Note that whatever
// your platform's int/long/long long happen to alias (e.g. int == s32) is
// accepted, so a literal like 300 counts as s32.
// ---------------------------------------------------------------------------
#define GILD_CONV_DECL(SRC, SNAME, DST, DNAME) \
    GILD_API DST gild_safe_cast_##SNAME##_to_##DNAME(SRC x, const char *file, s32 line, const char *func);

#define GILD_CONV_DECL_ALL_TO(SRC, SNAME) \
    GILD_CONV_DECL(SRC, SNAME, s8,     s8) \
    GILD_CONV_DECL(SRC, SNAME, u8,     u8) \
    GILD_CONV_DECL(SRC, SNAME, s16,    s16) \
    GILD_CONV_DECL(SRC, SNAME, u16,    u16) \
    GILD_CONV_DECL(SRC, SNAME, s32,    s32) \
    GILD_CONV_DECL(SRC, SNAME, u32,    u32) \
    GILD_CONV_DECL(SRC, SNAME, s64,    s64) \
    GILD_CONV_DECL(SRC, SNAME, u64,    u64) \
    GILD_CONV_DECL(SRC, SNAME, size_t, size_t)

GILD_CONV_DECL_ALL_TO(s8,     s8)
GILD_CONV_DECL_ALL_TO(u8,     u8)
GILD_CONV_DECL_ALL_TO(s16,    s16)
GILD_CONV_DECL_ALL_TO(u16,    u16)
GILD_CONV_DECL_ALL_TO(s32,    s32)
GILD_CONV_DECL_ALL_TO(u32,    u32)
GILD_CONV_DECL_ALL_TO(s64,    s64)
GILD_CONV_DECL_ALL_TO(u64,    u64)
GILD_CONV_DECL_ALL_TO(size_t, size_t)

#if defined(__GNUC__) || defined(__clang__)
__attribute__((error("gild_safe_cast: source type is not s8..s64, u8..u64 or size_t")))
#endif
void gild_cast_unsupported_source(const void *x, const char *file, s32 line, const char *func);

// Same size_t trick as GILD_SAFE_SIZE_FALLBACK above.
#define GILD_CAST_SIZE_SRC(DN, x) _Generic((x), \
    size_t:  gild_safe_cast_size_t_to_##DN, \
    default: gild_cast_unsupported_source)

#define gild_safe_cast(DN, x) _Generic((x), \
    s8:  gild_safe_cast_s8_to_##DN,   u8:  gild_safe_cast_u8_to_##DN, \
    s16: gild_safe_cast_s16_to_##DN,  u16: gild_safe_cast_u16_to_##DN, \
    s32: gild_safe_cast_s32_to_##DN,  u32: gild_safe_cast_u32_to_##DN, \
    s64: gild_safe_cast_s64_to_##DN,  u64: gild_safe_cast_u64_to_##DN, \
    default: GILD_CAST_SIZE_SRC(DN, x))((x), __FILE__, __LINE__, __func__)

#ifdef __cplusplus
}
#endif
