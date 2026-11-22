#include <stdint.h>
#include <math.h>
#include "gild_basic.h"
#include "gild_log.h"

// Overflow-checked arithmetic. See gild_basic.h for the API.
// Every add/sub/mult goes through a GILD_OVF_<op>(N, a, b, &r) helper that returns
// nonzero on overflow and ALWAYS stores the wrapped result in r, so that is what
// gets returned.
// Problems are reported through the library logger (gild_log_func), using the
// caller's file, line and function (passed in by the gild_safe_* macros in
// gild_basic.h) so the log points at the offending arithmetic, not here.
//
// The helpers come from one of two places:
//   - GCC >= 5 / Clang: the __builtin_{add,sub,mul}_overflow builtins (fast path).
//   - Anything else (MSVC, ...): portable C11 fallbacks below.
// Define GILD_PORTABLE_OVERFLOW to force the fallbacks on any compiler (useful for testing).

#if defined(GILD_PORTABLE_OVERFLOW)
	#define GILD_USE_BUILTIN_OVF 0
#elif defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 5)
	#define GILD_USE_BUILTIN_OVF 1
#else
	#define GILD_USE_BUILTIN_OVF 0
#endif

#if GILD_USE_BUILTIN_OVF

#define GILD_OVF_add(N, a, b, r)  __builtin_add_overflow(a, b, r)
#define GILD_OVF_sub(N, a, b, r)  __builtin_sub_overflow(a, b, r)
#define GILD_OVF_mult(N, a, b, r) __builtin_mul_overflow(a, b, r)

#else // portable fallbacks

// Unsigned types: wraparound is defined by the language. (T) casts bring u8/u16
// results back down from int after integer promotion.
#define OVF_ADDSUB_UNSIGNED_DEF(T, N) \
static s32 gild_add_ovf_##N(T a, T b, T *r){ *r = (T)(a + b); return *r < a; } \
static s32 gild_sub_ovf_##N(T a, T b, T *r){ *r = (T)(a - b); return a < b; }

// Signed types: do the arithmetic in the unsigned type UT (defined to wrap, so no
// signed-overflow UB), then detect overflow from the sign bits: add overflows when
// both operands differ in sign from the result, sub when the operands differ in
// sign and the result differs from a.
#define OVF_ADDSUB_SIGNED_DEF(T, UT, N) \
static s32 gild_add_ovf_##N(T a, T b, T *r){ \
	T s = (T)(UT)((UT)a + (UT)b); \
	*r = s; \
	return ((a ^ s) & (b ^ s)) < 0; \
} \
static s32 gild_sub_ovf_##N(T a, T b, T *r){ \
	T s = (T)(UT)((UT)a - (UT)b); \
	*r = s; \
	return ((a ^ b) & (a ^ s)) < 0; \
}

// Multiplication for types of 32 bits or fewer: the exact product fits in 64 bits
// (u64 for unsigned, since u32*u32 can exceed INT64_MAX), so compare it to the
// truncated result.
#define OVF_MULT_NARROW_SIGNED_DEF(T, UT, N) \
static s32 gild_mult_ovf_##N(T a, T b, T *r){ \
	s64 w = (s64)a * (s64)b; \
	*r = (T)(UT)(u64)w; \
	return w != (s64)*r; \
}
#define OVF_MULT_NARROW_UNSIGNED_DEF(T, N) \
static s32 gild_mult_ovf_##N(T a, T b, T *r){ \
	u64 w = (u64)a * (u64)b; \
	*r = (T)w; \
	return w != (u64)*r; \
}

OVF_ADDSUB_SIGNED_DEF(s8,  u8,  s8)
OVF_ADDSUB_UNSIGNED_DEF(u8,  u8)
OVF_ADDSUB_SIGNED_DEF(s16, u16, s16)
OVF_ADDSUB_UNSIGNED_DEF(u16, u16)
OVF_ADDSUB_SIGNED_DEF(s32, u32, s32)
OVF_ADDSUB_UNSIGNED_DEF(u32, u32)
OVF_ADDSUB_SIGNED_DEF(s64, u64, s64)
OVF_ADDSUB_UNSIGNED_DEF(u64, u64)
OVF_ADDSUB_UNSIGNED_DEF(size_t, size)

OVF_MULT_NARROW_SIGNED_DEF(s8,  u8,  s8)
OVF_MULT_NARROW_UNSIGNED_DEF(u8,  u8)
OVF_MULT_NARROW_SIGNED_DEF(s16, u16, s16)
OVF_MULT_NARROW_UNSIGNED_DEF(u16, u16)
OVF_MULT_NARROW_SIGNED_DEF(s32, u32, s32)
OVF_MULT_NARROW_UNSIGNED_DEF(u32, u32)

// 64-bit multiplication has no wider type to fall back on, so verify by dividing back.
// size_t uses the same check since its width differs between platforms.
static s32 gild_mult_ovf_u64(u64 a, u64 b, u64 *r){
	*r = a * b;
	return a != 0 && *r / a != b;
}
static s32 gild_mult_ovf_size(size_t a, size_t b, size_t *r){
	*r = a * b;
	return a != 0 && *r / a != b;
}
static s32 gild_mult_ovf_s64(s64 a, s64 b, s64 *r){
	*r = (s64)((u64)a * (u64)b);          // wrapped result, computed without signed UB
	if(a == 0 || b == 0){return 0;}
	// INT64_MIN * -1 is the one case where the division check below would itself overflow.
	if((a == -1 && b == INT64_MIN) || (b == -1 && a == INT64_MIN)){return 1;}
	return *r / b != a;
}

#define GILD_OVF_add(N, a, b, r)  gild_add_ovf_##N(a, b, r)
#define GILD_OVF_sub(N, a, b, r)  gild_sub_ovf_##N(a, b, r)
#define GILD_OVF_mult(N, a, b, r) gild_mult_ovf_##N(a, b, r)

#endif // GILD_USE_BUILTIN_OVF

// SAFE_LOG relies on the `file`, `line` and `func` parameters of the function it is used in.
#ifdef SAFE_QUIET
#define SAFE_LOG(code, ...) ((void)0)
#else
#define SAFE_LOG(code, ...) gild_log_func(code, file, line, func, __VA_ARGS__)
#endif

// N = type name, FMT/CAST = printf format and cast for the operands.
#define SAFE_BIN_DEF(T, N, FMT, CAST, OP, SYM) \
T gild_safe_##OP##_##N(T a, T b, const char *file, s32 line, const char *func){ \
	T r; \
	(void)file; (void)line; (void)func; \
	if(GILD_OVF_##OP(N, a, b, &r)) \
		SAFE_LOG(GILD_OVERFLOW, #N " " #OP " overflow: " FMT " " SYM " " FMT, \
		         (CAST)a, (CAST)b); \
	return r; \
}

// MINV = minimum value for signed types (0 for unsigned).
#define SAFE_INT_DEF(T, N, FMT, CAST, MINV) \
SAFE_BIN_DEF(T, N, FMT, CAST, add,  "+") \
SAFE_BIN_DEF(T, N, FMT, CAST, sub,  "-") \
SAFE_BIN_DEF(T, N, FMT, CAST, mult, "*") \
T gild_safe_div_##N(T a, T b, const char *file, s32 line, const char *func){ \
	(void)file; (void)line; (void)func; \
	if(b == 0){ \
		SAFE_LOG(GILD_INVALID_ARG, #N " div by zero: " FMT " / 0", (CAST)a); \
		return 0; \
	} \
	if((MINV) != 0 && a == (MINV) && b == (T)-1){ \
		SAFE_LOG(GILD_OVERFLOW, #N " div overflow: " FMT " / -1", (CAST)a); \
		return a; \
	} \
	return (T)(a / b); \
}

SAFE_INT_DEF(s8,     s8,   "%lld", long long, INT8_MIN)
SAFE_INT_DEF(u8,     u8,   "%llu", unsigned long long, 0)
SAFE_INT_DEF(s16,    s16,  "%lld", long long, INT16_MIN)
SAFE_INT_DEF(u16,    u16,  "%llu", unsigned long long, 0)
SAFE_INT_DEF(s32,    s32,  "%lld", long long, INT32_MIN)
SAFE_INT_DEF(u32,    u32,  "%llu", unsigned long long, 0)
SAFE_INT_DEF(s64,    s64,  "%lld", long long, INT64_MIN)
SAFE_INT_DEF(u64,    u64,  "%llu", unsigned long long, 0)
SAFE_INT_DEF(size_t, size, "%llu", unsigned long long, 0)

// Floats: "overflow" means a finite input produced an infinite result.
#define SAFE_FLT_BIN_DEF(OP, SYM, EXPR) \
float gild_safe_##OP##_f32(float a, float b, const char *file, s32 line, const char *func){ \
	float r = EXPR; \
	(void)file; (void)line; (void)func; \
	if(isinf(r) && !isinf(a) && !isinf(b)) \
		SAFE_LOG(GILD_OVERFLOW, "f32 " #OP " overflow: %g " SYM " %g", \
		         (double)a, (double)b); \
	return r; \
}

SAFE_FLT_BIN_DEF(add,  "+", a + b)
SAFE_FLT_BIN_DEF(sub,  "-", a - b)
SAFE_FLT_BIN_DEF(mult, "*", a * b)

float gild_safe_div_f32(float a, float b, const char *file, s32 line, const char *func){
	float r;
	(void)file; (void)line; (void)func;
	if(b == 0.0f){
		SAFE_LOG(GILD_INVALID_ARG, "f32 div by zero: %g / 0", (double)a);
		return 0.0f;
	}
	r = a / b;
	if(isinf(r) && !isinf(a))
		SAFE_LOG(GILD_OVERFLOW, "f32 div overflow: %g / %g", (double)a, (double)b);
	return r;
}

// ---------------------------------------------------------------------------
// Modulo (integers only). Same failure cases as div:
//   b == 0                -> GILD_INVALID_ARG, returns 0
//   MIN % -1 (signed)     -> GILD_OVERFLOW,    returns 0 (the true result; the
//                            operation itself is UB in C, so it is never executed)
// Signed results follow C semantics: the sign of the result is the sign of a.
// ---------------------------------------------------------------------------
#define SAFE_MOD_DEF(T, N, FMT, CAST, MINV) \
T gild_safe_mod_##N(T a, T b, const char *file, s32 line, const char *func){ \
	(void)file; (void)line; (void)func; \
	if(b == 0){ \
		SAFE_LOG(GILD_INVALID_ARG, #N " mod by zero: " FMT " %% 0", (CAST)a); \
		return 0; \
	} \
	if((MINV) != 0 && a == (MINV) && b == (T)-1){ \
		SAFE_LOG(GILD_OVERFLOW, #N " mod overflow: " FMT " %% -1", (CAST)a); \
		return 0; \
	} \
	return (T)(a % b); \
}

SAFE_MOD_DEF(s8,     s8,   "%lld", long long, INT8_MIN)
SAFE_MOD_DEF(u8,     u8,   "%llu", unsigned long long, 0)
SAFE_MOD_DEF(s16,    s16,  "%lld", long long, INT16_MIN)
SAFE_MOD_DEF(u16,    u16,  "%llu", unsigned long long, 0)
SAFE_MOD_DEF(s32,    s32,  "%lld", long long, INT32_MIN)
SAFE_MOD_DEF(u32,    u32,  "%llu", unsigned long long, 0)
SAFE_MOD_DEF(s64,    s64,  "%lld", long long, INT64_MIN)
SAFE_MOD_DEF(u64,    u64,  "%llu", unsigned long long, 0)
SAFE_MOD_DEF(size_t, size, "%llu", unsigned long long, 0)

#define SAFE_BIT_DEF(T, UT, N, BITS) \
void gild_safe_bit_set_##N(T *p, u32 n, const char *file, s32 line, const char *func){ \
	(void)file; (void)line; (void)func; \
	if(n >= (BITS)){ \
		SAFE_LOG(GILD_INVALID_ARG, #N " bit_set index out of range: %llu (valid: 0..%llu)", \
		         (unsigned long long)n, (unsigned long long)((BITS) - 1)); \
		return; \
	} \
	*p = (T)((UT)*p | (UT)((UT)1 << n)); \
} \
void gild_safe_bit_clear_##N(T *p, u32 n, const char *file, s32 line, const char *func){ \
	(void)file; (void)line; (void)func; \
	if(n >= (BITS)){ \
		SAFE_LOG(GILD_INVALID_ARG, #N " bit_clear index out of range: %llu (valid: 0..%llu)", \
		         (unsigned long long)n, (unsigned long long)((BITS) - 1)); \
		return; \
	} \
	*p = (T)((UT)*p & (UT)~(UT)((UT)1 << n)); \
} \
bool gild_safe_bit_check_##N(T x, u32 n, const char *file, s32 line, const char *func){ \
	(void)file; (void)line; (void)func; \
	if(n >= (BITS)){ \
		SAFE_LOG(GILD_INVALID_ARG, #N " bit_check index out of range: %llu (valid: 0..%llu)", \
		         (unsigned long long)n, (unsigned long long)((BITS) - 1)); \
		return false; \
	} \
	return (((UT)x >> n) & 1u) != 0; \
}

SAFE_BIT_DEF(s8,  u8,  s8,  8)
SAFE_BIT_DEF(u8,  u8,  u8,  8)
SAFE_BIT_DEF(s16, u16, s16, 16)
SAFE_BIT_DEF(u16, u16, u16, 16)
SAFE_BIT_DEF(s32, u32, s32, 32)
SAFE_BIT_DEF(u32, u32, u32, 32)
SAFE_BIT_DEF(s64, u64, s64, 64)
SAFE_BIT_DEF(u64, u64, u64, 64)

// ---------------------------------------------------------------------------
// Checked conversions: gild_safe_cast_<src>_to_<dst>(x).
// The value is converted (wrapping, exactly like a C cast), then verified:
//   - converting back must give the original value, and
//   - if the signedness differs, the sign must not have flipped.
// On failure the wrapped value is returned and GILD_OVERFLOW is logged, matching
// how gild_safe_add/sub/mult behave.
// Kinds: Si = signed source/dest, Un = unsigned. The extra sign condition is:
//   Un->Si: result must be >= 0     Si->Un: source must be >= 0     same: none
// ---------------------------------------------------------------------------
#define GILD_CHK_Si_Si 1
#define GILD_CHK_Un_Un 1
#define GILD_CHK_Un_Si (r >= 0)
#define GILD_CHK_Si_Un (x >= 0)

#define GILD_CONV_DEF(SRC, SNAME, SKIND, SFMT, SCAST, DST, DNAME, DKIND) \
DST gild_safe_cast_##SNAME##_to_##DNAME(SRC x, const char *file, s32 line, const char *func){ \
	DST r = (DST)x; \
	(void)file; (void)line; (void)func; \
	if(!(((SRC)r == x) && GILD_CHK_##SKIND##_##DKIND)) \
		SAFE_LOG(GILD_OVERFLOW, #SNAME " -> " #DNAME " conversion out of range: " SFMT, \
		         (SCAST)x); \
	return r; \
}

#define GILD_CONV_ALL_TO(SRC, SNAME, SKIND, SFMT, SCAST) \
	GILD_CONV_DEF(SRC, SNAME, SKIND, SFMT, SCAST, s8,     s8,     Si) \
	GILD_CONV_DEF(SRC, SNAME, SKIND, SFMT, SCAST, u8,     u8,     Un) \
	GILD_CONV_DEF(SRC, SNAME, SKIND, SFMT, SCAST, s16,    s16,    Si) \
	GILD_CONV_DEF(SRC, SNAME, SKIND, SFMT, SCAST, u16,    u16,    Un) \
	GILD_CONV_DEF(SRC, SNAME, SKIND, SFMT, SCAST, s32,    s32,    Si) \
	GILD_CONV_DEF(SRC, SNAME, SKIND, SFMT, SCAST, u32,    u32,    Un) \
	GILD_CONV_DEF(SRC, SNAME, SKIND, SFMT, SCAST, s64,    s64,    Si) \
	GILD_CONV_DEF(SRC, SNAME, SKIND, SFMT, SCAST, u64,    u64,    Un) \
	GILD_CONV_DEF(SRC, SNAME, SKIND, SFMT, SCAST, size_t, size_t, Un)

GILD_CONV_ALL_TO(s8,     s8,     Si, "%lld", long long)
GILD_CONV_ALL_TO(u8,     u8,     Un, "%llu", unsigned long long)
GILD_CONV_ALL_TO(s16,    s16,    Si, "%lld", long long)
GILD_CONV_ALL_TO(u16,    u16,    Un, "%llu", unsigned long long)
GILD_CONV_ALL_TO(s32,    s32,    Si, "%lld", long long)
GILD_CONV_ALL_TO(u32,    u32,    Un, "%llu", unsigned long long)
GILD_CONV_ALL_TO(s64,    s64,    Si, "%lld", long long)
GILD_CONV_ALL_TO(u64,    u64,    Un, "%llu", unsigned long long)
GILD_CONV_ALL_TO(size_t, size_t, Un, "%llu", unsigned long long)
