#include <stdlib.h>
#include "gild_mem.h"
#include "gild_log.h"

// No need to check for allocation since it crashes when an allocation fails. My hubris is not large enough to recover from failing to get memory

void *gild_xmalloc_impl(size_t size, const char *file, s32 line, const char *func){
	if(size == 0){
		gild_log_func(GILD_INDEX, file, line, func, "xmalloc called with size 0");
		return NULL;
	}
	void *p = malloc(size);
	if(!p){
		gild_log_func(GILD_ALLOC, file, line, func, "malloc failed for %zu bytes", size);
	}
	return p;
}

void *gild_xcalloc_impl(size_t count, size_t size, const char *file, s32 line, const char *func){
	if(count == 0 || size == 0){
		gild_log_func(GILD_INDEX, file, line, func,
			"xcalloc called with zero size (count=%zu, size=%zu)", count, size);
		return NULL;
	}
	void *p = calloc(count, size);
	if(!p){
		gild_log_func(GILD_ALLOC, file, line, func, "calloc failed for %zu x %zu bytes", count, size);
	}
	return p;
}

void *gild_xrealloc_impl(void *ptr, size_t size, const char *file, s32 line, const char *func){
	if(size == 0){
		gild_log_func(GILD_INDEX, file, line, func, "xrealloc called with size 0, freeing block");
		free(ptr);
		return NULL;
	}
	void *p = realloc(ptr, size);
	if(!p){
		gild_log_func(GILD_ALLOC, file, line, func, "realloc failed for %zu bytes", size);
	}
	return p;
}
