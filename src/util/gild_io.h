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

// Read-or-write with a sticky error flag.
//
// One function both saves and loads a struct, so the two can never drift apart, and
// there's no error check after every field: once any call fails, io->ok goes false
// and stays false, and every later call does nothing. Check once at the end.
//
//     bool io_thing(struct Gild_IO *io, struct Thing *t){
//         gild_io_u32(io, &t->id);
//         gild_io_s16(io, &t->hp);
//         for(u32 i = 0; i < 8; i++){gild_io_u32(io, &t->slots[i]);}
//         return io->ok;
//     }
//
//     struct Gild_IO io = gild_io_begin(f, false);  // false = reading
//     struct Thing tmp = {0};                        // read into a zeroed copy
//     io_thing(&io, &tmp);
//     if(!gild_io_end(&io)){ ... throw tmp away ... }
//
// Every call also returns io->ok, for the rare case you need to stop straight away
// (e.g. before allocating from a count you just read).
// You can set io->ok = false yourself when a value you read is out of range;
// everything after it is then skipped too.
struct Gild_IO{
	FILE *f;
	bool writing; // true: write *v to the file, false: read from the file into *v
	bool ok;      // starts true, goes false on the first failure and stays false
};

// ok starts false if f is NULL, so a failed fopen falls straight through to the end check.
GILD_API struct Gild_IO gild_io_begin(FILE *f, bool writing);

// Call when done. When writing it flushes, so a full disk is caught here instead of
// missed (fwrite only fills a buffer). Doesn't close f, and fclose can still fail.
GILD_API bool gild_io_end(struct Gild_IO *io);

// Header: 8 magic bytes (written as-is), u64 timestamp, u32 version.
// Writing: writes *version and the current time.
// Reading: fails only on a wrong magic. It hands back the file's version in *version
// so you decide which versions you can still load.
// timestamp may be NULL; otherwise it receives the time written or read (for save menus).
GILD_API bool gild_io_header(struct Gild_IO *io, const char mag[8], u32 *version, u64 *timestamp);

// All multi-byte integers are stored little-endian regardless of the host.
// To write a computed value (an array count), put it in a variable first; on a read
// the same variable receives it. Reads leave *v untouched on failure.
GILD_API bool gild_io_u8 (struct Gild_IO *io, u8  *v);
GILD_API bool gild_io_u16(struct Gild_IO *io, u16 *v);
GILD_API bool gild_io_u32(struct Gild_IO *io, u32 *v);
GILD_API bool gild_io_u64(struct Gild_IO *io, u64 *v);

GILD_API bool gild_io_s8 (struct Gild_IO *io, s8  *v);
GILD_API bool gild_io_s16(struct Gild_IO *io, s16 *v);
GILD_API bool gild_io_s32(struct Gild_IO *io, s32 *v);
GILD_API bool gild_io_s64(struct Gild_IO *io, s64 *v);

GILD_API bool gild_io_bytes(struct Gild_IO *io, void *buf, size_t n);

// Strings: u32 byte length, then the bytes (no terminator).
// Writing: NULL and "" are both written as length 0.
// Reading: mallocs the string into *s (caller frees); length 0 reads back as NULL.
// A read fails if the stored length is over max_len, so a corrupt file can't ask
// for a huge allocation (max_len is ignored when writing).
// A read overwrites *s without freeing it, so read into a zeroed struct.
// On failure *s is left untouched.
GILD_API bool gild_io_str(struct Gild_IO *io, char **s, u32 max_len);

#ifdef __cplusplus
}
#endif
