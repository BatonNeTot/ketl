//🫖ketl
#ifndef ketl_gc_memory_h
#define ketl_gc_memory_h

#include "ketl/memory.h"
#include "ketl/type.h"

#include "containers/vector.h"
#include "containers/tree_map.h"

#include "ketl/utils.h"

KETL_DEFINE(ketl_gc_info) {
    ketl_type* pType;
    bool flagUsage;
};

KETL_NAMED_VECTOR_DECLARATION(objects, void*)
KETL_NAMED_TREE_MAP_DECLARATION(object_info_map, void*, ketl_gc_info)

KETL_DEFINE(ketl_gc) {
    const ketl_allocator *pAllocator;
    objects vRootObjects;
    objects vCollectBuffer;
    object_info_map mObjectInfo;
    bool flagUsage;
};

void ketl_gc_init(ketl_gc* pGc, const ketl_allocator *pAllocator);

void ketl_gc_deinit(ketl_gc* pGc);

void ketl_gc_reg_root(ketl_gc* pGc, void* pObject, ketl_type* pType);

void ketl_gc_collect(ketl_gc* pGc);

#endif // ketl_gc_memory_h
