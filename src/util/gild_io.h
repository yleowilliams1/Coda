#pragma once
#include <stdio.h>
#include "gild_basic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GILD_CONFIG_LINE_MAX    256
#define GILD_CONFIG_SECTION_MAX 64
#define GILD_CONFIG_KEY_MAX     64
#define GILD_CONFIG_VALUE_MAX   128
struct Gild_ConfigPack{
	char line[GILD_CONFIG_LINE_MAX];
	char current_section[GILD_CONFIG_SECTION_MAX];
	char key[GILD_CONFIG_KEY_MAX];
	char value[GILD_CONFIG_VALUE_MAX];
};
typedef void (*Gild_ConfigParser)(const struct Gild_ConfigPack *pack, void *ptr);

// All paths in gild are UTF-8. On Windows they are converted to UTF-16 and
// opened with _wfopen, so non-ASCII (e.g. Japanese) file and folder names work.
// Use gild_fopen instead of fopen in your own code for the same behaviour.
GILD_API FILE *gild_fopen(const char *path, const char *mode);

GILD_API bool gild_config(void *ptr, const char *path, Gild_ConfigParser func);

// Returns "<base>/<num>.<format>" (malloc'd, caller frees) or NULL. num must be >= 0.
GILD_API char *gild_format_path(const char *base, const char *format, s32 num);
GILD_API bool  gild_parent_dir_exists(const char *path);

// Header: 8 magic bytes (written as-is), u64 timestamp, u32 version.
// gild_read_header fails on a wrong magic OR a wrong version.
GILD_API bool gild_write_header(FILE *f, const char mag[8], u32 version);
GILD_API bool gild_read_header(FILE *f, const char mag[8], u32 version);

GILD_API bool gild_write_bytes(FILE *f, const void *buf, size_t n);
GILD_API bool gild_read_bytes(FILE *f, void *buf, size_t n);

// Strings: u32 byte length, then the bytes (no terminator).
// NULL and "" are both written as length 0 and both read back as NULL.
// gild_read_str mallocs the result (caller frees). It fails if the stored length
// is over max_len, so a corrupt file can't ask for a huge allocation.
// On failure *out is left untouched.
GILD_API bool gild_write_str(FILE *f, const char *s);
GILD_API bool gild_read_str(FILE *f, char **out, u32 max_len);

// All multi-byte integers are stored little-endian regardless of the host.
GILD_API bool gild_write_u8(FILE *f, u8 v);
GILD_API bool gild_write_u16(FILE *f, u16 v);
GILD_API bool gild_write_u32(FILE *f, u32 v);
GILD_API bool gild_write_u64(FILE *f, u64 v);

GILD_API bool gild_write_s8(FILE *f, s8 v);
GILD_API bool gild_write_s16(FILE *f, s16 v);
GILD_API bool gild_write_s32(FILE *f, s32 v);
GILD_API bool gild_write_s64(FILE *f, s64 v);

GILD_API bool gild_read_u8(FILE *f, u8 *out);
GILD_API bool gild_read_u16(FILE *f, u16 *out);
GILD_API bool gild_read_u32(FILE *f, u32 *out);
GILD_API bool gild_read_u64(FILE *f, u64 *out);

GILD_API bool gild_read_s8(FILE *f, s8 *out);
GILD_API bool gild_read_s16(FILE *f, s16 *out);
GILD_API bool gild_read_s32(FILE *f, s32 *out);
GILD_API bool gild_read_s64(FILE *f, s64 *out);

#ifdef __cplusplus
}
#endif
