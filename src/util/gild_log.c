#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <io.h>
	#define IS_TTY(f) _isatty(_fileno(f))
#else
	#include <unistd.h>
	#define IS_TTY(f) isatty(fileno(f))
#endif

#include "gild_basic.h"
#include "gild_log.h"
#include "gild_io.h"

static enum GILD_LOG_LVL severity(enum GILD_LOG_CODE code);
static const char *sev_label(enum GILD_LOG_LVL lvl);
static const char *code_label(enum GILD_LOG_CODE code);
static const char *lvl_col(enum GILD_LOG_LVL lvl);
static void fatal_crash(void);

static FILE *log_file = NULL;
static s32 console_enabled = 1;
static void (*crash_hook)(void) = NULL;

/* ---------- portability helpers ---------- */

static void log_localtime(time_t t, struct tm *out){
#if defined(_WIN32)
	localtime_s(out, &t);      /* MSVC argument order */
#else
	localtime_r(&t, out);
#endif
}

/* 1 if stderr is a terminal that will understand ANSI codes */
static s32 log_use_color(void){
	static s32 cached = -1;
	if(cached != -1) return cached;

	cached = 0;
	if(getenv("NO_COLOR")) return cached;   /* respect NO_COLOR convention */
	if(!IS_TTY(stderr))    return cached;

#if defined(_WIN32)
	{
		HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
		DWORD mode = 0;
		if(h == INVALID_HANDLE_VALUE || !GetConsoleMode(h, &mode)) return cached;
		if(!SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return cached;
	}
#endif
	cached = 1;
	return cached;
}

/* ---------- public API ---------- */

/* Turn stderr output on/off (turn off while ncurses owns the terminal) */
void gild_log_set_console(s32 enabled){ console_enabled = enabled; }

/* Called before abort() on a crash-level log (e.g. to call endwin()) */
void gild_log_set_crash_hook(void (*hook)(void)){ crash_hook = hook; }

void gild_log_init(const char *path){
	if(log_file){fclose(log_file); log_file = NULL;}
	if(!path){GILD_LOG(GILD_IS_NULL, "Passed NULL path");return;}
	log_file = gild_fopen(path, "w");
	if(!log_file){GILD_LOG(GILD_IS_NULL, "Path %s failed to open", path);return;}
}

void gild_log_func(enum GILD_LOG_CODE code, const char *file, s32 line,
              const char *func, const char *fmt, ...)
{
	char msg[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	enum GILD_LOG_LVL lvl = severity(code);
	const char *str_code = code_label(code);
	const char *str_lvl  = sev_label(lvl);

	char timebuf[32] = "0000-00-00 00:00:00";
	time_t t = time(NULL);
	struct tm tm_buf;
	log_localtime(t, &tm_buf);
	strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_buf);

	/* Console */
	if(console_enabled){
		if(log_use_color()){
			fprintf(stderr, "[%s:%s%s%s] %s (%s:%d in %s())\n",
				str_code, lvl_col(lvl), str_lvl, GILD_ANSI_RESET,
				msg, file, line, func);
		} else {
			fprintf(stderr, "[%s:%s] %s (%s:%d in %s())\n",
				str_code, str_lvl, msg, file, line, func);
		}
	}

	/* File */
	if(log_file){
		fprintf(log_file, "%s | %s:%s | code=%d | %s:%d in %s() | %s\n",
			timebuf, str_code, str_lvl, (s32)code, file, line, func, msg);
		fflush(log_file);
	}

	if(lvl == GILD_CRASH){
		fatal_crash();
	}
}

/* ---------- internals ---------- */

static void fatal_crash(void){
	/* Let the app restore the terminal first (endwin(), etc.).
	   Clear the hook so a GILD_LOG() inside it can't recurse. */
	if(crash_hook){
		void (*hook)(void) = crash_hook;
		crash_hook = NULL;
		hook();
	}
	console_enabled = 1;   /* make sure the fatal message is visible */

	s32 color = log_use_color();

	fprintf(stderr, "%s[FATAL]%s A fatal error occurred. See error.log for details.\n",
		color ? GILD_ANSI_RED : "", color ? GILD_ANSI_RESET : "");

	if(log_file){
		fprintf(log_file, "== FATAL: process aborting ==\n");
		fflush(log_file);
		fclose(log_file);
		log_file = NULL;
	}

	abort();
}

static enum GILD_LOG_LVL severity(enum GILD_LOG_CODE code){
	switch(code){
		#define GILD_X_(name, lvl) case GILD_##name: return GILD_##lvl;
		GILD_LOG_LIST(GILD_X_)
		#undef GILD_X_
		default: return GILD_OK;
	}
}

static const char *sev_label(enum GILD_LOG_LVL lvl){
	switch(lvl){
		#define GILD_X_(name, col) case GILD_##name: return #name;
		GILD_LOG_LEVELS(GILD_X_)
		#undef GILD_X_
		default: return "UNKNOWN";
	}
}

static const char *code_label(enum GILD_LOG_CODE code){
	switch(code){
		#define GILD_X_(name, lvl) case GILD_##name: return #name;
		GILD_LOG_LIST(GILD_X_)
		#undef GILD_X_
		default: return "UNKNOWN";
	}
}

static const char *lvl_col(enum GILD_LOG_LVL lvl){
	switch(lvl){
		#define GILD_X_(name, col) case GILD_##name: return col;
		GILD_LOG_LEVELS(GILD_X_)
		#undef GILD_X_
		default: return GILD_ANSI_RESET;
	}
}
