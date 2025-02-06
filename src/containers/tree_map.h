//🫖ketl
#ifndef ketl_containers_tree_map_h
#define ketl_containers_tree_map_h

#include "ketl/utils.h"

#include "memory_impl.h"

#define KETL_TREE_MAP_DECLARATION(kType, vType) KETL_NAMED_TREE_MAP_DECLARATION(KETL_CONCAT(kType,_,vType,_tree_map), kType, vType)
#define KETL_NAMED_TREE_MAP_DECLARATION(name, kType, vType)\
KETL_DEFINE(KETL_CONCAT(name,_node)) {\
    union {\
    uint32_t leftOffset;\
    uint32_t nextOffset;\
    };\
    uint32_t rightOffset;\
	uint16_t height;\
	kType key;\
	vType value;\
};\
KETL_DEFINE(name) {\
    const ketl_allocator* pAllocator;\
	KETL_CONCAT(name,_node)* pNodes;\
	uint32_t freeNodeOffset;\
	uint32_t rootOffset;\
	uint32_t size;\
	uint32_t capacity;\
};\
void KETL_CONCAT(name,_init)(name* pMap, uint32_t initialCapacity, const ketl_allocator* pAllocator);\
void KETL_CONCAT(name,_deinit)(name* pMap);\
KETL_CONCAT(name,_node)* KETL_CONCAT(name,_get_or_insert_copy)(name* pMap, kType key, vType value);\
KETL_CONCAT(name,_node)* KETL_CONCAT(name,_get_or_insert_ref)(name* pMap, kType key, vType const* pValue);\
KETL_CONCAT(name,_node)* KETL_CONCAT(name,_erase)(name* pMap, kType key);\

#define KETL_TREE_MAP_DEFINITION(kType, vType, kLess) KETL_NAMED_TREE_MAP_DEFINITION(KETL_CONCAT(kType,_,vType,_tree_map), kType, vType, kLess)
#define KETL_NAMED_TREE_MAP_DEFINITION(name, kType, vType, kLess)\
void KETL_CONCAT(name,_init)(name* pMap, uint32_t initialCapacity, const ketl_allocator* pAllocator) {\
    const uint32_t arraySize = sizeof(KETL_CONCAT(name,_node)) * initialCapacity;\
    \
    KETL_CONCAT(name,_node)* pNodes = ketl_alloc(pAllocator, arraySize);\
    \
    *pMap = (name){\
        .pAllocator = pAllocator,\
        .pNodes = pNodes,\
        .freeNodeOffset = 0,\
        .rootOffset = (uint32_t)(-1),\
        .size = 0,\
        .capacity = initialCapacity};\
    \
    --initialCapacity;\
    for (uint32_t i = 0u; i < initialCapacity; ++i) {\
        pNodes[i].nextOffset = i + 1;\
    }\
    pNodes[initialCapacity].nextOffset = (uint32_t)(-1);\
}\
void KETL_CONCAT(name,_deinit)(name* pMap) {\
    ketl_free(pMap->pAllocator, pMap->pNodes);\
}\
static uint16_t KETL_CONCAT(__,name,_height)(KETL_CONCAT(name,_node)* pNodes, uint32_t nodeOffset) {\
    if (nodeOffset == (uint32_t)(-1)) {\
        return 0;\
    }\
    KETL_CONCAT(name,_node)* pNode = pNodes + nodeOffset;\
    return pNode->height;\
}\
static int64_t KETL_CONCAT(__,name,_get_balance)(KETL_CONCAT(name,_node)* pNodes, uint32_t nodeOffset) {\
    if (nodeOffset == (uint32_t)(-1)) {\
        return 0;\
    }\
    KETL_CONCAT(name,_node)* pNode = pNodes + nodeOffset;\
    return KETL_CONCAT(__,name,_height)(pNodes, pNode->rightOffset) - KETL_CONCAT(__,name,_height)(pNodes, pNode->leftOffset);\
}\
static uint32_t KETL_CONCAT(__,name,_rotate_left)(KETL_CONCAT(name,_node)* pNodes, uint32_t nodeOffset) {\
    KETL_CONCAT(name,_node) *pX = pNodes + nodeOffset;\
    uint32_t pYOffset = pX->rightOffset;\
    KETL_CONCAT(name,_node) *pY = pNodes + pYOffset;\
    uint32_t T2Offset = pY->leftOffset;\
\
    pY->leftOffset = nodeOffset;\
    pX->rightOffset = T2Offset;\
\
    pX->height = 1 + KETL_MAX(KETL_CONCAT(__,name,_height)(pNodes, pX->leftOffset), KETL_CONCAT(__,name,_height)(pNodes, pX->rightOffset));\
    pY->height = 1 + KETL_MAX(KETL_CONCAT(__,name,_height)(pNodes, pY->leftOffset), KETL_CONCAT(__,name,_height)(pNodes, pY->rightOffset));\
\
    return pYOffset;\
}\
static uint32_t KETL_CONCAT(__,name,_rotate_right)(KETL_CONCAT(name,_node)* pNodes, uint32_t nodeOffset) {\
    KETL_CONCAT(name,_node) *pX = pNodes + nodeOffset;\
    uint32_t pYOffset = pX->leftOffset;\
    KETL_CONCAT(name,_node) *pY = pNodes + pYOffset;\
    uint32_t T2Offset = pY->rightOffset;\
\
    pY->rightOffset = nodeOffset;\
    pX->leftOffset = T2Offset;\
\
    pX->height = 1 + KETL_MAX(KETL_CONCAT(__,name,_height)(pNodes, pX->leftOffset), KETL_CONCAT(__,name,_height)(pNodes, pX->rightOffset));\
    pY->height = 1 + KETL_MAX(KETL_CONCAT(__,name,_height)(pNodes, pY->leftOffset), KETL_CONCAT(__,name,_height)(pNodes, pY->rightOffset));\
\
    return pYOffset;\
}\
static uint32_t KETL_CONCAT(__,name,_balance)(KETL_CONCAT(name,_node)* pNodes, uint32_t nodeOffset) {\
    KETL_CONCAT(name,_node)* pNode = pNodes + nodeOffset;\
    int32_t balance = KETL_CONCAT(__,name,_get_balance)(pNodes, nodeOffset);\
    \
    if (balance < -1) {\
        if (KETL_CONCAT(__,name,_get_balance)(pNodes, pNode->leftOffset) <= 0) {\
            return KETL_CONCAT(__,name,_rotate_right)(pNodes, nodeOffset);\
        } else {\
            pNode->leftOffset = KETL_CONCAT(__,name,_rotate_left)(pNodes, pNode->leftOffset);\
            return KETL_CONCAT(__,name,_rotate_right)(pNodes, nodeOffset);\
        }\
    }\
\
    if (balance > 1) {\
        if (KETL_CONCAT(__,name,_get_balance)(pNodes, pNode->rightOffset) >= 0) {\
            return KETL_CONCAT(__,name,_rotate_left)(pNodes, nodeOffset);\
        } else {\
            pNode->rightOffset = KETL_CONCAT(__,name,_rotate_right)(pNodes, pNode->rightOffset);\
            return KETL_CONCAT(__,name,_rotate_left)(pNodes, nodeOffset);\
        }\
    }\
\
    return nodeOffset;\
}\
static uint32_t KETL_CONCAT(__,name,_get_or_insert_impl)(name* pMap, uint32_t nodeOffset, kType key, uint32_t* pFoundOffset) {\
    KETL_CONCAT(name,_node)* pNodes = pMap->pNodes;\
    if (nodeOffset == (uint32_t)(-1)) {\
        if (pMap->freeNodeOffset == (uint32_t)(-1)) {\
            /* TODO allocate more */\
        }\
\
        uint32_t newNodeOffset = pMap->freeNodeOffset;\
        KETL_CONCAT(name,_node)* pNewNode = pNodes + newNodeOffset;\
        pMap->freeNodeOffset = pNewNode->nextOffset;\
\
        pNewNode->leftOffset = (uint32_t)(-1);\
        pNewNode->rightOffset = (uint32_t)(-1);\
        pNewNode->height = 1;\
        pNewNode->key = key;\
\
        return *pFoundOffset = newNodeOffset;\
    }\
\
    KETL_CONCAT(name,_node)* pNode = pNodes + nodeOffset;\
    if (kLess(key, pNode->key)) {\
        pNode->leftOffset = KETL_CONCAT(__,name,_get_or_insert_impl)(pMap, pNode->leftOffset, key, pFoundOffset);\
    } else if (kLess(pNode->key, key)) {\
        pNode->rightOffset = KETL_CONCAT(__,name,_get_or_insert_impl)(pMap, pNode->rightOffset, key, pFoundOffset);\
    } else {\
        return *pFoundOffset = nodeOffset;\
    }\
\
    pNode->height = 1 + KETL_MAX(KETL_CONCAT(__,name,_height)(pNodes, pNode->leftOffset), KETL_CONCAT(__,name,_height)(pNodes, pNode->rightOffset));\
\
    return KETL_CONCAT(__,name,_balance)(pNodes, nodeOffset);\
}\
KETL_CONCAT(name,_node)* KETL_CONCAT(name,_get_or_insert_copy)(name* pMap, kType key, vType value) {\
    uint32_t freeNodeOffset = pMap->freeNodeOffset;\
    uint32_t foundOffset;\
    pMap->rootOffset = KETL_CONCAT(__,name,_get_or_insert_impl)(pMap, pMap->rootOffset, key, &foundOffset);\
\
    KETL_CONCAT(name,_node)* pFoundNode = pMap->pNodes + foundOffset;\
    if (freeNodeOffset == foundOffset) {\
        pFoundNode->value = value;\
    }\
    \
    return pFoundNode;\
}\
KETL_CONCAT(name,_node)* KETL_CONCAT(name,_get_or_insert_ref)(name* pMap, kType key, vType const* pValue) {\
    uint32_t freeNodeOffset = pMap->freeNodeOffset;\
    uint32_t foundOffset;\
    pMap->rootOffset = KETL_CONCAT(__,name,_get_or_insert_impl)(pMap, pMap->rootOffset, key, &foundOffset);\
\
    KETL_CONCAT(name,_node)* pFoundNode = pMap->pNodes + foundOffset;\
    if (freeNodeOffset == foundOffset) {\
        pFoundNode->value = *pValue;\
    }\
    \
    return pFoundNode;\
}\
static uint64_t KETL_CONCAT(__,name,_erase_leftmost_impl)(KETL_CONCAT(name,_node)* pNodes, uint32_t nodeOffset, uint32_t* pFoundOffset) {\
    KETL_CONCAT(name,_node)* pNode = pNodes + nodeOffset;\
    if (pNode->leftOffset == (uint32_t)(-1)) {\
        *pFoundOffset = nodeOffset;\
        return pNode->rightOffset;\
    } else {\
        pNode->leftOffset = KETL_CONCAT(__,name,_erase_leftmost_impl)(pNodes, pNode->leftOffset, pFoundOffset);\
    }\
\
    pNode->height = 1 + KETL_MAX(KETL_CONCAT(__,name,_height)(pNodes, pNode->leftOffset), KETL_CONCAT(__,name,_height)(pNodes, pNode->rightOffset));\
\
    return KETL_CONCAT(__,name,_balance)(pNodes, nodeOffset);\
}\
static uint64_t KETL_CONCAT(__,name,_erase_impl)(KETL_CONCAT(name,_node)* pNodes, uint32_t nodeOffset, kType key, uint32_t* pFoundOffset) {\
    if (nodeOffset == (uint32_t)(-1)) {\
        return nodeOffset;\
    }\
\
    KETL_CONCAT(name,_node)* pNode = pNodes + nodeOffset;\
    if (kLess(key, pNode->key)) {\
        pNode->leftOffset = KETL_CONCAT(__,name,_erase_impl)(pNodes, pNode->leftOffset, key, pFoundOffset);\
    } else if (kLess(pNode->key, key)) {\
        pNode->rightOffset = KETL_CONCAT(__,name,_erase_impl)(pNodes, pNode->rightOffset, key, pFoundOffset);\
    } else {\
        if (pNode->leftOffset == (uint32_t)(-1)) {\
            *pFoundOffset = nodeOffset;\
            return pNode->rightOffset;\
        } \
        if (pNode->rightOffset == (uint32_t)(-1)) {\
            *pFoundOffset = nodeOffset;\
            return pNode->leftOffset;\
        }\
\
        uint32_t leftOffset = pNode->leftOffset;\
        uint32_t rightOffset = KETL_CONCAT(__,name,_erase_leftmost_impl)(pNodes, pNode->rightOffset, pFoundOffset);\
\
        uint32_t replacementOffset = *pFoundOffset;\
        *pFoundOffset = nodeOffset;\
\
        nodeOffset = replacementOffset;\
        pNode = pNodes + replacementOffset;\
\
        pNode->rightOffset = rightOffset;\
        pNode->leftOffset = leftOffset;\
    }\
\
    pNode->height = 1 + KETL_MAX(KETL_CONCAT(__,name,_height)(pNodes, pNode->leftOffset), KETL_CONCAT(__,name,_height)(pNodes, pNode->rightOffset));\
\
    return KETL_CONCAT(__,name,_balance)(pNodes, nodeOffset);\
}\
KETL_CONCAT(name,_node)* KETL_CONCAT(name,_erase)(name* pMap, kType key) {\
    uint32_t foundOffset = (uint32_t)(-1);\
    KETL_CONCAT(name,_node)* pNodes = pMap->pNodes; \
    pMap->rootOffset = KETL_CONCAT(__,name,_erase_impl)(pNodes, pMap->rootOffset, key, &foundOffset);\
    if (foundOffset == (uint32_t)(-1)) {\
        return NULL;\
    }\
    \
    KETL_CONCAT(name,_node)* pNode = pNodes + foundOffset;\
    pNode->nextOffset = pMap->freeNodeOffset;\
    pMap->freeNodeOffset = foundOffset;\
    return pNode;\
}\

#endif // ketl_containers_tree_map_h