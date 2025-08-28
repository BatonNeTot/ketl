//🫖ketl
#include "gc_memory.h"

#include "type_impl.h"

KETL_VECTOR_DEFINITION(objects, void*)
KETL_TREE_MAP_DEFINITION(object_info_map, void*, ketl_gc_info, ANN_LESS)

void ketl_gc_init(ketl_gc* pGc, const ketl_allocator *p_allocator) {
    objects_init(&pGc->vRootObjects, 16, p_allocator);
    objects_init(&pGc->vCollectBuffer, 16, p_allocator);
    object_info_map_init(&pGc->mObjectInfo, 16, p_allocator);
    pGc->flagUsage = false;
}

void ketl_gc_deinit(ketl_gc* pGc) {
    object_info_map_deinit(&pGc->mObjectInfo);
    objects_deinit(&pGc->vCollectBuffer);
    objects_deinit(&pGc->vRootObjects);
}

void* ketl_gc_create(ketl_gc* pGc, ketl_type* pType, uint8_t flags) {
    void* pObject = ketl_alloc(pGc->p_allocator, pType->size);
    ketl_gc_reg(pGc, pObject, pType, flags | KETL_GC_FREE_AFTER_USE);
    return pObject;
}

void ketl_gc_reg(ketl_gc* pGc, void* pObject, ketl_type* pType, uint8_t flags) {
    ketl_gc_info info = {
        .pType = pType,
        .flagUsage = pGc->flagUsage,
        .freeAfterUse = flags & KETL_GC_FREE_AFTER_USE,
    };
    object_info_map_get_or_insert_ref(&pGc->mObjectInfo, pObject, &info);
    if (flags & KETL_GC_ROOT) {
        objects_push_back_copy(&pGc->vRootObjects, pObject);
    }
}

static void* find_object_start(ketl_gc* pGc, const void* pInsideObject, ketl_gc_info** ppInfo) {
    if (pGc->mObjectInfo.size == 0) {
        return NULL;
    }

    object_info_map_node* pNodes = pGc->mObjectInfo.pNodes;
    object_info_map_node* pNode = pNodes + pGc->mObjectInfo.rootOffset;
    ANN_FOREVER {
        if (pInsideObject < pNode->key) {
            if (pNode->leftOffset == (uint32_t)(-1)) {
                return NULL;
            }
            pNode = pNodes + pNode->leftOffset;
            continue;
        }
        if (pNode->key < pInsideObject) {
            if (pNode->rightOffset == (uint32_t)(-1)) {
                // TODO check if it is in the range of a type and return pNode->key
                return NULL;
            }
            pNode = pNodes + pNode->rightOffset;
            continue;
        }
        *ppInfo = &pNode->value;
        return pNode->key;
    }
}

static void mark_objects(ketl_gc* pGc) {
    bool flagUsage = (pGc->flagUsage ^= true);
    objects_resize(&pGc->vCollectBuffer, pGc->vRootObjects.size);
    ketl_memcpy(pGc->vCollectBuffer.p_data, pGc->vRootObjects.p_data, pGc->vRootObjects.size * sizeof(void*));
    while (pGc->vCollectBuffer.size) {
        void* pObject = pGc->vRootObjects.p_data[--pGc->vCollectBuffer.size];
        ketl_gc_info* pInfo;
        void* pRoot = find_object_start(pGc, pObject, &pInfo);
        if (pRoot && pInfo->flagUsage != flagUsage) {
            pInfo->flagUsage = flagUsage;
            objects_push_back_copy(&pGc->vRootObjects, pInfo->pType);
            // TODO collect fields to vCollectBuffer
        }
    }
}

static uint32_t visit_node_and_swipe(ketl_gc* pGc, uint32_t nodeOffset) {
    if (nodeOffset == (uint32_t)(-1)) {
        return (uint32_t)(-1);
    }

    object_info_map_node* pNodes = pGc->mObjectInfo.pNodes;
    object_info_map_node* pNode = pNodes + nodeOffset;
    pNode->leftOffset = visit_node_and_swipe(pGc, pNode->leftOffset);
    pNode->rightOffset = visit_node_and_swipe(pGc, pNode->rightOffset);

    if (pNode->value.flagUsage != pGc->flagUsage) {
        // TODO place for destructor if needed
        if (pNode->value.freeAfterUse) {
            ketl_free(pGc->p_allocator, pNode->key);
        }
        return (uint32_t)(-1);
    }

    pNode->height = 1 + ANN_MAX(__object_info_map_height(pNodes, pNode->leftOffset), __object_info_map_height(pNodes, pNode->rightOffset));

    return __object_info_map_balance(pNodes, nodeOffset);
}

static void swipe_objects(ketl_gc* pGc) {
    objects_clear(&pGc->vCollectBuffer);
    pGc->mObjectInfo.rootOffset = visit_node_and_swipe(pGc, pGc->mObjectInfo.rootOffset);
}

void ketl_gc_collect(ketl_gc* pGc) {
    mark_objects(pGc);
    swipe_objects(pGc);
}
