//🫖ketl
#ifndef ketl_gc_memory_h
#define ketl_gc_memory_h

#include "ketl/memory.h"

#include "ketl/utils.h"


KETL_DEFINE(ketl_gc) {
    const ketl_allocator *pAllocator;
};

void ketl_gc_init(ketl_gc* pGc);

void ketl_gc_deinit(ketl_gc* pGc);

void ketl_gc_collect(ketl_gc* pGc);

#endif // ketl_gc_memory_h
