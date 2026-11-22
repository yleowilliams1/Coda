#pragma once
#include "gild_basic.h"

#ifdef __cplusplus
extern "C" {
#endif

GILD_API void *gild_xmalloc_impl(size_t size, const char *file, s32 line, const char *func);
GILD_API void *gild_xcalloc_impl(size_t count, size_t size, const char *file, s32 line, const char *func);
GILD_API void *gild_xrealloc_impl(void *ptr, size_t size, const char *file, s32 line, const char *func);

#define GILD_XMALLOC(size)       (void *)gild_xmalloc_impl(size, __FILE__, __LINE__, __func__)
#define GILD_XCALLOC(count, size) (void *)gild_xcalloc_impl(count, size, __FILE__, __LINE__, __func__)
#define GILD_XREALLOC(ptr, size) (void *)gild_xrealloc_impl(ptr, size, __FILE__, __LINE__, __func__)

#ifdef __cplusplus
}
#endif
