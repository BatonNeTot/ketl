#include "ketl/ketl.h"

#include "type_impl.h"
#include "gc_memory.h"

KETL_DEFINE(ketl_state) {
    const ketl_allocator* pAllocator;
    ketl_gc gc;
};
