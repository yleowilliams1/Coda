#pragma once
#include "gild_basic.h"

/* Old MSVC lacks __func__ */
#if defined(_MSC_VER) && _MSC_VER < 1900
	#define __func__ __FUNCTION__
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GILD_ANSI_WHITE_ON_RED "\x1b[97;41m"
#define GILD_ANSI_RED          "\x1b[31m"
#define GILD_ANSI_YELLOW       "\x1b[33m"
#define GILD_ANSI_GREEN        "\x1b[32m"
#define GILD_ANSI_RESET        "\x1b[0m"

// X-macro lists. The entry macro is passed in as a parameter (X), so nothing
// named X is ever #defined or #undef'd in your code.
#define GILD_LOG_LEVELS(X)\
	X(OK,       GILD_ANSI_GREEN)\
	X(WARNING,  GILD_ANSI_YELLOW)\
	X(CRITICAL, GILD_ANSI_RED)\
	X(CRASH,    GILD_ANSI_WHITE_ON_RED)

#define GILD_LOG_LIST(X)\
    /* Lifecycle */\
    X(INIT,            OK)\
    X(LOAD,            OK)\
    X(FREE,            OK)\
    X(START,           OK)\
    X(STOP,            OK)\
    X(SHUTDOWN,        OK)\
    X(RESET,           OK)\
    X(RELOAD,          OK)\
    /* Data access */\
    X(SET,             OK)\
    X(GET,             OK)\
    X(INSERT,          OK)\
    X(REMOVE,          OK)\
    X(UPDATE,          OK)\
    X(FIND,            OK)\
    X(COPY,            OK)\
    X(MOVE,            OK)\
    X(RESIZE,          OK)\
    X(CLEAR,           OK)\
    /* User / misc */\
    X(INTERACT,        OK)\
    X(INFO,            OK)\
    X(DEBUG,           OK)\
    X(TRACE,           OK)\
    X(CONFIG,          OK)\
    X(SAVE,            OK)\
    X(CONNECT,         OK)\
    X(DISCONNECT,      OK)\
    /* Warnings */\
    X(INDEX,           WARNING)\
    X(IS_NULL,         WARNING)\
    X(DEPRECATED,      WARNING)\
    X(TIMEOUT,         WARNING)\
    X(RETRY,           WARNING)\
    X(OVERFLOW,        WARNING)\
    X(UNDERFLOW,       WARNING)\
    X(TRUNCATED,       WARNING)\
    X(DUPLICATE,       WARNING)\
    X(NOT_FOUND,       WARNING)\
    X(EMPTY,           WARNING)\
    X(FULL,            WARNING)\
    X(LOW_MEMORY,      WARNING)\
    X(SLOW,            WARNING)\
    X(UNSUPPORTED,     WARNING)\
    X(DEFAULTED,       WARNING)\
    X(IGNORED,         WARNING)\
    /* Critical errors */\
    X(NO_FILE,         CRITICAL)\
    X(PARSE,           CRITICAL)\
    X(WRITE,           CRITICAL)\
    X(READ,            CRITICAL)\
    X(OPEN,            CRITICAL)\
    X(CLOSE,           CRITICAL)\
    X(SEEK,            CRITICAL)\
    X(PERMISSION,      CRITICAL)\
    X(INVALID_ARG,     CRITICAL)\
    X(INVALID_STATE,   CRITICAL)\
    X(INVALID_FORMAT,  CRITICAL)\
    X(OUT_OF_BOUNDS,   CRITICAL)\
    X(TYPE_MISMATCH,   CRITICAL)\
    X(VERSION_MISMATCH,CRITICAL)\
    X(CHECKSUM,        CRITICAL)\
    X(CORRUPT,         CRITICAL)\
    X(CONNECTION,      CRITICAL)\
    X(NETWORK,         CRITICAL)\
    X(SYNTAX,          CRITICAL)\
    X(DEPENDENCY,      CRITICAL)\
    X(LOCK,            CRITICAL)\
    X(THREAD,          CRITICAL)\
    X(INIT_FAIL,       CRITICAL)\
    X(NOT_IMPLEMENTED, CRITICAL)\
    X(UNREACHABLE,     CRITICAL)\
    /* Crashes */\
    X(ALLOC,           CRASH)\
    X(ABORT,           CRASH)\
    X(ASSERT,          CRASH)\
    X(PANIC,           CRASH)\
    X(SEGFAULT,        CRASH)\
    X(STACK_OVERFLOW,  CRASH)\
    X(DEADLOCK,        CRASH)\
    X(FATAL,           CRASH)\
    X(UNHANDLED,       CRASH)

#define GILD_LOG_ENUM_ENTRY_(name, unused) GILD_##name,
enum GILD_LOG_LVL{
	GILD_LOG_LEVELS(GILD_LOG_ENUM_ENTRY_)
};
enum GILD_LOG_CODE{
	GILD_LOG_LIST(GILD_LOG_ENUM_ENTRY_)
};
#undef GILD_LOG_ENUM_ENTRY_

GILD_API void gild_log_init(const char *path);
GILD_API void gild_log_set_console(s32 enabled);
GILD_API void gild_log_set_crash_hook(void (*hook)(void));
GILD_API void gild_log_func(enum GILD_LOG_CODE code, const char *file, s32 line,
                            const char *func, const char *fmt, ...);

#define GILD_LOG(code, fmt, ...) \
	gild_log_func(code, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
