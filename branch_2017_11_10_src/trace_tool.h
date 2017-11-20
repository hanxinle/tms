#ifndef __TRACE_TOOL_H__
#define __TRACE_TOOL_H__

#include <stdlib.h>

//#define malloc TraceMalloc
//#define free TraceFree
//#define realloc TraceRealloc

void* TraceMalloc(size_t size);
void TraceFree(void* ptr);
void* TraceRealloc(void *ptr, size_t size);

#endif // __TRACE_TOOL_H__
