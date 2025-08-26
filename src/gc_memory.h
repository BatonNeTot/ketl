//🫖ketl
#ifndef ketl_gc_memory_h
#define ketl_gc_memory_h

#include "ketl/memory.h"
#include "ketl/type.h"

#include "containers/vector.h"
#include "containers/tree_map.h"

#include "ketl/utils.h"

ANN_DEFINE(ketl_gc_info) {
    ketl_type* pType;
    bool flagUsage;
    bool freeAfterUse;
};

KETL_VECTOR_DECLARATION(objects, void*)
KETL_TREE_MAP_DECLARATION(object_info_map, void*, ketl_gc_info)

ANN_DEFINE(ketl_gc) {
    const ketl_allocator *pAllocator;
    objects vRootObjects;
    objects vCollectBuffer;
    object_info_map mObjectInfo;
    bool flagUsage;
};

void ketl_gc_init(ketl_gc* pGc, const ketl_allocator *pAllocator);

void ketl_gc_deinit(ketl_gc* pGc);

#define KETL_GC_ROOT            0x01
#define KETL_GC_FREE_AFTER_USE  0x02

void* ketl_gc_create(ketl_gc* pGc, ketl_type* pType, uint8_t flags);

void ketl_gc_reg(ketl_gc* pGc, void* pObject, ketl_type* pType, uint8_t flags);

void ketl_gc_collect(ketl_gc* pGc);

#endif // ketl_gc_memory_h
