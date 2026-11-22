#include <string.h>
#include "gild_mem.h"
#include "gild_log.h"
#include "gild_string.h"

char *gild_copy_alloc_str(const char *name, size_t len){
	if(!name){GILD_LOG(GILD_IS_NULL, "Passed NULL string"); return NULL;}
	char *copy = GILD_XCALLOC(1, gild_safe_add(len, (size_t)1));
	memcpy(copy, name, len);
	copy[len] = '\0';
	return copy;
}
