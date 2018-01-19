#include <stdint.h>

#include <iostream>
#include <map>

#include "common_define.h"

using namespace std;

#include "trace_tool.h"

#ifdef malloc
#undef malloc
#endif

#ifdef free
#undef free
#endif

#ifdef realloc
#undef realloc
#endif

double malloc_size = 0.0;

map<uint64_t, double> ptr_size;

void* TraceMalloc(size_t size)
{
    void* p = ::malloc(size);

    double kb = (double)size/1024.0;

    cout << LMSG << "malloc " << kb << " KB, pointer:" << p << endl;

    malloc_size += kb;

    ptr_size[(uint64_t)p] = kb;

    return p;
}

void TraceFree(void* ptr)
{
    ::free(ptr);

    double kb = ptr_size[(uint64_t)ptr];

    malloc_size -= kb;

    cout << LMSG << "free " << kb << " KB, pointer:" << ptr << endl;

    ptr_size.erase((uint64_t)ptr);
}

void* TraceRealloc(void *ptr, size_t size)
{
    void* p = ::realloc(ptr, size);

    double kb = ptr_size[(uint64_t)ptr]/1024.0;

    malloc_size -= kb;

    kb = (double)size/1024.0;

    malloc_size += kb;

    ptr_size.erase((uint64_t)ptr);

    ptr_size[(uint64_t)p] = kb;

    return p;
}
