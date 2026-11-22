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

bool gild_write_header(FILE *f, const char mag[8], u32 version){
	if(!f || !mag){return false;}
	// Magic is written byte-for-byte, so it reads back the same on any host.
	if(gild_write_bytes(f, mag, 8) && gild_write_u64(f, (u64)time(NULL)) && gild_write_u32(f, version)){
		return true;
	}
	GILD_LOG(GILD_WRITE, "Failed to write header");
	return false;
}
bool gild_read_header(FILE *f, const char mag[8], u32 version){
	if(!f || !mag){return false;}

	char got_mag[8];
	u64 timestamp;
	u32 ver;

	if(!(gild_read_bytes(f, got_mag, 8) && gild_read_u64(f, &timestamp) && gild_read_u32(f, &ver))){
		GILD_LOG(GILD_READ, "Failed to read header");
		return false;
	}
	if(memcmp(got_mag, mag, 8) != 0){GILD_LOG(GILD_READ, "Wrong magic number"); return false;}
	if(ver != version){
		GILD_LOG(GILD_VERSION_MISMATCH, "Wrong version (file %u, expected %u)", ver, version);
		return false;
	}
	return true;
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
bool gild_write_bytes(FILE *f, const void *buf, size_t n) {
	if (!f || (n > 0 && !buf)) { return false; }
	return (n == 0 || fwrite(buf, 1, n, f) == n) ? true : false;
}

bool gild_read_bytes(FILE *f, void *buf, size_t n) {
	if (!f || (n > 0 && !buf)) { return false; }
	return (n == 0 || fread(buf, 1, n, f) == n) ? true : false;
}

bool gild_write_u8(FILE *f, u8 v) {
	if (!f) { return false; }
	return fwrite(&v, 1, 1, f) == 1 ? true : false;
}

bool gild_write_u16(FILE *f, u16 v) {
	if (!f) { return false; }
	u8 b[2] = {
		(u8)(v & 0xFF),
		(u8)((v >> 8) & 0xFF)
	};
	return fwrite(b, 1, 2, f) == 2 ? true : false;
}

bool gild_write_u32(FILE *f, u32 v) {
	if (!f) { return false; }
	u8 b[4] = {
		(u8)(v & 0xFF),
		(u8)((v >> 8)  & 0xFF),
		(u8)((v >> 16) & 0xFF),
		(u8)((v >> 24) & 0xFF)
	};
	return fwrite(b, 1, 4, f) == 4 ? true : false;
}

bool gild_write_u64(FILE *f, u64 v) {
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

bool gild_write_s8(FILE *f, s8 v) {
	return gild_write_u8(f, (u8)v);
}

bool gild_write_s16(FILE *f, s16 v) {
	return gild_write_u16(f, (u16)v);
}

bool gild_write_s32(FILE *f, s32 v) {
	return gild_write_u32(f, (u32)v);
}

bool gild_write_s64(FILE *f, s64 v) {
	return gild_write_u64(f, (u64)v);
}

bool gild_read_u8(FILE *f, u8 *out) {
	if (!f || !out) { return false; }
	return fread(out, 1, 1, f) == 1 ? true : false;
}

bool gild_read_u16(FILE *f, u16 *out) {
	u8 b[2];
	if (!f || !out) { return false; }
	if (fread(b, 1, 2, f) != 2) { return false; }
	*out = (u16)((u16)b[0] | ((u16)b[1] << 8));
	return true;
}

bool gild_read_u32(FILE *f, u32 *out) {
	u8 b[4];
	if (!f || !out) { return false; }
	if (fread(b, 1, 4, f) != 4) { return false; }
	*out = (u32)b[0]
	     | ((u32)b[1] << 8)
	     | ((u32)b[2] << 16)
	     | ((u32)b[3] << 24);
	return true;
}

bool gild_read_u64(FILE *f, u64 *out) {
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

bool gild_read_s8(FILE *f, s8 *out) {
	u8 v;
	if (!gild_read_u8(f, &v)) { return false; }
	if (out) { *out = (s8)v; }
	return true;
}

bool gild_read_s16(FILE *f, s16 *out) {
	u16 v;
	if (!gild_read_u16(f, &v)) { return false; }
	if (out) { *out = (s16)v; }
	return true;
}

bool gild_read_s32(FILE *f, s32 *out) {
	u32 v;
	if (!gild_read_u32(f, &v)) { return false; }
	if (out) { *out = (s32)v; }
	return true;
}

bool gild_read_s64(FILE *f, s64 *out) {
	u64 v;
	if (!gild_read_u64(f, &v)) { return false; }
	if (out) { *out = (s64)v; }
	return true;
}
