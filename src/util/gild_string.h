#pragma once
#include "gild_basic.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns a malloc'd, NUL-terminated copy of the first `len` chars of `name`
// (caller frees), or NULL if `name` is NULL.
GILD_API char *gild_copy_alloc_str(const char *name, size_t len);

#ifdef __cplusplus
}
#endif
