#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// Platform-specific parts of this file:
//   - opening files: on Windows, paths are UTF-8 in gild and get converted to
//     UTF-16 for _wfopen, because plain fopen uses the legacy code page;
//   - "is this path a directory?", which ISO C cannot answer.
#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <wchar.h>
#else
	#include <sys/stat.h>
#endif

#include "gild_basic.h"
#include "gild_log.h"
#include "gild_mem.h"
#include "gild_string.h"
#include "gild_io.h"

#if defined(_WIN32)
// UTF-8 -> malloc'd, NUL-terminated UTF-16 (caller frees). NULL if `s` is not
// valid UTF-8. MB_ERR_INVALID_CHARS makes bad input fail instead of silently
// turning into U+FFFD and opening the wrong file.
static wchar_t *utf8_to_wide(const char *s){
	int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
	if(n <= 0){return NULL;}
	wchar_t *w = GILD_XMALLOC(gild_safe_mult(gild_safe_cast(size_t, n), sizeof(wchar_t)));
	if(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, w, n) != n){
		free(w);
		return NULL;
	}
	return w;
}
#endif

FILE *gild_fopen(const char *path, const char *mode){
	if(!path || !mode){GILD_LOG(GILD_IS_NULL, "Passed NULL path or mode to gild_fopen"); return NULL;}
#if defined(_WIN32)
	wchar_t *wpath = utf8_to_wide(path);
	wchar_t *wmode = utf8_to_wide(mode);
	FILE *f = NULL;
	if(wpath && wmode){
		f = _wfopen(wpath, wmode);
	} else {
		GILD_LOG(GILD_INVALID_FORMAT, "Path or mode is not valid UTF-8");
	}
	free(wpath);
	free(wmode);
	return f;
#else
	// Linux and macOS take UTF-8 paths natively.
	return fopen(path, mode);
#endif
}

// Raw little-endian reads and writes. Static: outside this file use gild_io_*.
static bool write_bytes(FILE *f, const void *buf, size_t n) {
	if (!f || (n > 0 && !buf)) { return false; }
	return (n == 0 || fwrite(buf, 1, n, f) == n) ? true : false;
}

static bool read_bytes(FILE *f, void *buf, size_t n) {
	if (!f || (n > 0 && !buf)) { return false; }
	return (n == 0 || fread(buf, 1, n, f) == n) ? true : false;
}

static bool write_u8(FILE *f, u8 v) {
	if (!f) { return false; }
	return fwrite(&v, 1, 1, f) == 1 ? true : false;
}

static bool write_u16(FILE *f, u16 v) {
	if (!f) { return false; }
	u8 b[2] = {
		(u8)(v & 0xFF),
		(u8)((v >> 8) & 0xFF)
	};
	return fwrite(b, 1, 2, f) == 2 ? true : false;
}

static bool write_u32(FILE *f, u32 v) {
	if (!f) { return false; }
	u8 b[4] = {
		(u8)(v & 0xFF),
		(u8)((v >> 8)  & 0xFF),
		(u8)((v >> 16) & 0xFF),
		(u8)((v >> 24) & 0xFF)
	};
	return fwrite(b, 1, 4, f) == 4 ? true : false;
}

static bool write_u64(FILE *f, u64 v) {
	if (!f) { return false; }
	u8 b[8] = {
		(u8)(v & 0xFF),
		(u8)((v >> 8)  & 0xFF),
		(u8)((v >> 16) & 0xFF),
		(u8)((v >> 24) & 0xFF),
		(u8)((v >> 32) & 0xFF),
		(u8)((v >> 40) & 0xFF),
		(u8)((v >> 48) & 0xFF),
		(u8)((v >> 56) & 0xFF)
	};
	return fwrite(b, 1, 8, f) == 8 ? true : false;
}

static bool write_s8(FILE *f, s8 v) {
	return write_u8(f, (u8)v);
}

static bool write_s16(FILE *f, s16 v) {
	return write_u16(f, (u16)v);
}

static bool write_s32(FILE *f, s32 v) {
	return write_u32(f, (u32)v);
}

static bool write_s64(FILE *f, s64 v) {
	return write_u64(f, (u64)v);
}

static bool read_u8(FILE *f, u8 *out) {
	if (!f || !out) { return false; }
	return fread(out, 1, 1, f) == 1 ? true : false;
}

static bool read_u16(FILE *f, u16 *out) {
	u8 b[2];
	if (!f || !out) { return false; }
	if (fread(b, 1, 2, f) != 2) { return false; }
	*out = (u16)((u16)b[0] | ((u16)b[1] << 8));
	return true;
}

static bool read_u32(FILE *f, u32 *out) {
	u8 b[4];
	if (!f || !out) { return false; }
	if (fread(b, 1, 4, f) != 4) { return false; }
	*out = (u32)b[0]
	     | ((u32)b[1] << 8)
	     | ((u32)b[2] << 16)
	     | ((u32)b[3] << 24);
	return true;
}

static bool read_u64(FILE *f, u64 *out) {
	u8 b[8];
	if (!f || !out) { return false; }
	if (fread(b, 1, 8, f) != 8) { return false; }
	*out = (u64)b[0]
	     | ((u64)b[1] << 8)
	     | ((u64)b[2] << 16)
	     | ((u64)b[3] << 24)
	     | ((u64)b[4] << 32)
	     | ((u64)b[5] << 40)
	     | ((u64)b[6] << 48)
	     | ((u64)b[7] << 56);
	return true;
}

static bool read_s8(FILE *f, s8 *out) {
	u8 v;
	if (!read_u8(f, &v)) { return false; }
	if (out) { *out = (s8)v; }
	return true;
}

static bool read_s16(FILE *f, s16 *out) {
	u16 v;
	if (!read_u16(f, &v)) { return false; }
	if (out) { *out = (s16)v; }
	return true;
}

static bool read_s32(FILE *f, s32 *out) {
	u32 v;
	if (!read_u32(f, &v)) { return false; }
	if (out) { *out = (s32)v; }
	return true;
}

static bool read_s64(FILE *f, s64 *out) {
	u64 v;
	if (!read_u64(f, &v)) { return false; }
	if (out) { *out = (s64)v; }
	return true;
}

static bool write_str(FILE *f, const char *s){
	if(!f){return false;}
	u64 len = s ? strlen(s) : 0;
	if(len > UINT32_MAX){
		GILD_LOG(GILD_INVALID_ARG, "String too long to write (%llu bytes)", (unsigned long long)len);
		return false;
	}
	return write_u32(f, (u32)len) && write_bytes(f, s, (size_t)len);
}

static bool read_str(FILE *f, char **out, u32 max_len){
	if(!f || !out){return false;}
	u32 len;
	if(!read_u32(f, &len)){return false;}
	if(len == 0){*out = NULL; return true;}
	if(len > max_len){
		GILD_LOG(GILD_CORRUPT, "Stored string length %u is over the limit of %u", len, max_len);
		return false;
	}
	char *s = GILD_XMALLOC(gild_safe_add((size_t)len, (size_t)1));
	if(!read_bytes(f, s, len)){
		free(s);
		return false;
	}
	s[len] = '\0';
	*out = s;
	return true;
}

// ---------------------------------------------------------------------------
// Read-or-write with a sticky error flag, see gild_io.h. These are the only way in
// from outside this file.
// ---------------------------------------------------------------------------

struct Gild_IO gild_io_begin(FILE *f, bool writing){
	if(!f){GILD_LOG(GILD_IS_NULL, "Passed NULL file to gild_io_begin");}
	struct Gild_IO io = {f, writing, f != NULL};
	return io;
}

bool gild_io_end(struct Gild_IO *io){
	if(!io){GILD_LOG(GILD_IS_NULL, "Passed NULL io to gild_io_end"); return false;}
	if(io->ok && io->writing && (fflush(io->f) != 0 || ferror(io->f))){
		GILD_LOG(GILD_WRITE, "Failed to flush file (disk full?)");
		io->ok = false;
	}
	return io->ok;
}

// False if an earlier call already failed. A missing argument is a bug in the caller,
// so it's logged and fails the io too.
static bool io_ready(struct Gild_IO *io, bool args_ok){
	if(!io){GILD_LOG(GILD_IS_NULL, "Passed NULL io"); return false;}
	if(!io->ok){return false;}
	if(!args_ok){GILD_LOG(GILD_IS_NULL, "Passed NULL value to gild_io"); io->ok = false; return false;}
	return true;
}

bool gild_io_header(struct Gild_IO *io, const char mag[8], u32 *version, u64 *timestamp){
	if(!io_ready(io, mag != NULL && version != NULL)){return false;}

	if(io->writing){
		u64 now = (u64)time(NULL);
		// Magic is written byte-for-byte, so it reads back the same on any host.
		io->ok = write_bytes(io->f, mag, 8) && write_u64(io->f, now) && write_u32(io->f, *version);
		if(!io->ok){GILD_LOG(GILD_WRITE, "Failed to write header"); return false;}
		if(timestamp){*timestamp = now;}
		return true;
	}

	char got_mag[8];
	u64 when;
	u32 ver;
	io->ok = read_bytes(io->f, got_mag, 8) && read_u64(io->f, &when) && read_u32(io->f, &ver);
	if(!io->ok){GILD_LOG(GILD_READ, "Failed to read header"); return false;}
	if(memcmp(got_mag, mag, 8) != 0){
		GILD_LOG(GILD_READ, "Wrong magic number");
		io->ok = false;
		return false;
	}
	*version = ver;
	if(timestamp){*timestamp = when;}
	return true;
}

// The eight integer versions are identical apart from the type, so they're stamped
// out here rather than copy-pasted (a copy-paste slip like calling write_u16 inside
// gild_io_u32 would be easy to miss).
#define GILD_IO_INT(T) \
	bool gild_io_##T(struct Gild_IO *io, T *v){ \
		if(!io_ready(io, v != NULL)){return false;} \
		io->ok = io->writing ? write_##T(io->f, *v) : read_##T(io->f, v); \
		return io->ok; \
	}
GILD_IO_INT(u8)
GILD_IO_INT(u16)
GILD_IO_INT(u32)
GILD_IO_INT(u64)
GILD_IO_INT(s8)
GILD_IO_INT(s16)
GILD_IO_INT(s32)
GILD_IO_INT(s64)
#undef GILD_IO_INT

bool gild_io_bytes(struct Gild_IO *io, void *buf, size_t n){
	if(!io_ready(io, buf != NULL || n == 0)){return false;}
	io->ok = io->writing ? write_bytes(io->f, buf, n) : read_bytes(io->f, buf, n);
	return io->ok;
}

bool gild_io_str(struct Gild_IO *io, char **s, u32 max_len){
	if(!io_ready(io, s != NULL)){return false;}
	io->ok = io->writing ? write_str(io->f, *s) : read_str(io->f, s, max_len);
	return io->ok;
}

// Strips leading space/tab and trailing space/tab/CR, in place.
// sscanf's %63[^=] keeps the whitespace before '=' (e.g. "flag_path " from
// "flag_path = x"), which would break every strcmp against a clean lookup string.
static void trim_in_place(char *s){
	char *start = s;
	while(*start == ' ' || *start == '\t'){start++;}

	u64 len = strlen(start);
	while(len > 0){
		char c = start[gild_safe_sub(len, 1)];
		if(c != ' ' && c != '\t' && c != '\r'){break;}
		len = gild_safe_sub(len, 1);
	}
	start[len] = '\0';

	if(start != s){
		memmove(s, start, gild_safe_add(len, 1)); // +1 to include the null terminator
	}
}

// Parses "key = value" lines and "[section]" headers, calling func once per pair.
// Blank lines and lines starting with '#' or ';' are skipped (leading whitespace allowed).
// Lines longer than GILD_CONFIG_LINE_MAX-1 are skipped with a warning (never half-parsed).
// "key =" gives an empty value.
bool gild_config(void *ptr, const char *path, Gild_ConfigParser func){
	struct Gild_ConfigPack pack = {0};
	// ptr may legitimately be NULL: config never dereferences it, it only passes it on.
	if(!func){GILD_LOG(GILD_IS_NULL, "Passed NULL loader to config"); return false;}
	if(!path){GILD_LOG(GILD_IS_NULL, "Passed NULL path to config"); return false;}

	FILE *f = gild_fopen(path, "r");
	if(!f){GILD_LOG(GILD_NO_FILE, "Failed to open path %s. Aborting", path); return false;}

	bool first = true;
	while(fgets(pack.line, sizeof(pack.line), f)){
		char *p = pack.line;
		u64 n = strlen(p);

		// No newline and not at EOF means the line didn't fit: drain the rest and skip it.
		if(n > 0 && p[gild_safe_sub(n, 1)] != '\n' && !feof(f)){
			int c;
			while((c = fgetc(f)) != '\n' && c != EOF){}
			GILD_LOG(GILD_TRUNCATED, "Line too long in %s (max %d chars), skipped", path, GILD_CONFIG_LINE_MAX - 2);
			first = false;
			continue;
		}

		// Skip a UTF-8 BOM at the start of the file.
		if(first && (u8)p[0] == 0xEF && (u8)p[1] == 0xBB && (u8)p[2] == 0xBF){p += 3;}
		first = false;

		while(*p == ' ' || *p == '\t'){p++;}
		if(*p == '\0' || *p == '\n' || *p == '\r' || *p == '#' || *p == ';'){continue;}

		if(*p == '['){
			if(sscanf(p, "[%63[^]]]", pack.current_section) != 1){
				GILD_LOG(GILD_PARSE, "Malformed section header in %s", path);
			}
			continue;
		}

		int got = sscanf(p, "%63[^=]=%127[^\n]", pack.key, pack.value);
		if(got < 1 || !strchr(p, '=')){continue;}   // no '=' (or nothing before it)
		if(got == 1){pack.value[0] = '\0';}         // "key =" with nothing after
		trim_in_place(pack.key);
		trim_in_place(pack.value);
		func(&pack, ptr);
	}

	bool ok = !ferror(f);
	fclose(f);
	if(!ok){GILD_LOG(GILD_READ, "Read error while reading %s", path); return false;}
	return true;
}

char *gild_format_path(const char *base, const char *format, s32 num){
	if(!base){GILD_LOG(GILD_IS_NULL, "Passed NULL base string"); return NULL;}
	if(!format){GILD_LOG(GILD_IS_NULL, "Passed NULL format string"); return NULL;}
	if(num < 0){GILD_LOG(GILD_INVALID_ARG, "Negative file number %d", num); return NULL;}

	u64 base_len = strlen(base);
	// Empty base means a relative "<num>.<ext>", not "/<num>.<ext>" (that would be the filesystem root).
	const char *sep = "";
	if(base_len > 0){
		char last = base[gild_safe_sub(base_len, 1)];
#if defined(_WIN32)
		if(last != '/' && last != '\\'){sep = "/";}
#else
		if(last != '/'){sep = "/";}
#endif
	}

	int needed = snprintf(NULL, 0, "%s%s%d.%s", base, sep, num, format);
	if(needed < 0){
		GILD_LOG(GILD_PARSE, "failed to compute length");
		return NULL;
	}

	u64 alloc_len = gild_safe_add((u64)needed, 1);
	char *file = GILD_XMALLOC(alloc_len);

	int wrote = snprintf(file, alloc_len, "%s%s%d.%s", base, sep, num, format);
	if(wrote < 0 || (u64)wrote >= alloc_len){
		free(file);
		GILD_LOG(GILD_WRITE, "Failed to write path buffer");
		return NULL;
	}

	return file;
}

static bool is_directory(const char *p){
#if defined(_WIN32)
	wchar_t *wp = utf8_to_wide(p);
	if(!wp){return false;}
	DWORD attr = GetFileAttributesW(wp);
	free(wp);
	return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
	struct stat st;
	return (stat(p, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

static char *last_separator(char *s){
	char *slash = strrchr(s, '/');
#if defined(_WIN32)
	char *back = strrchr(s, '\\');
	if(back && (!slash || back > slash)){slash = back;}
#endif
	return slash;
}

// Checks the directory that CONTAINS `path` (for "is it safe to write this file?").
bool gild_parent_dir_exists(const char *path){
	if(!path){GILD_LOG(GILD_IS_NULL, "Passed NULL path to gild_parent_dir_exists"); return false;}

	// Heap copy instead of a fixed buffer: UTF-8 paths with non-ASCII names
	// (3 bytes per Japanese character) hit a small fixed limit quickly.
	// Searching for '/' and '\' byte-by-byte is safe in UTF-8, since those
	// bytes never appear inside a multi-byte character.
	char *buf = gild_copy_alloc_str(path, strlen(path));

	char *slash = last_separator(buf);
	if(slash == NULL){
		// no directory component, implies current directory
		free(buf);
		return true;
	}
	// Keep the root intact: "/x" -> "/", "C:\x" -> "C:\"; otherwise truncate at the separator.
	if(slash == buf || (slash == buf + 2 && buf[1] == ':')){
		slash[1] = '\0';
	} else {
		*slash = '\0';
	}
	bool exists = is_directory(buf);
	free(buf);
	return exists;
}
