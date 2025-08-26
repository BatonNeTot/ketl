//🫖ketl
#ifndef ketl_containers_vector_h
#define ketl_containers_vector_h

#include "ketl/utils.h"

#include "memory_impl.h"

#define KETL_VECTOR_DECLARATION(name, type)\
ANN_DEFINE(name) {\
    const ketl_allocator* pAllocator;\
	type* pData;\
	uint32_t size;\
	uint32_t capacity;\
};\
void ANN_CONCAT(name,_init)(name* pVector, uint32_t initialCapacity, const ketl_allocator* pAllocator);\
void ANN_CONCAT(name,_deinit)(name* pVector);\
void ANN_CONCAT(name,_clear)(name* pVector);\
void ANN_CONCAT(name,_resize)(name* pVector, uint32_t newSize);\
void ANN_CONCAT(name,_reserve)(name* pVector, uint32_t newCapacity);\
type* ANN_CONCAT(name,_push_back_copy)(name* pVector, type value);\
type* ANN_CONCAT(name,_push_back_ref)(name* pVector, type const* pValue);\
type* ANN_CONCAT(name,_push_back_ref_n)(name* pVector, type const* pValues, uint32_t size);\

#define KETL_VECTOR_DEFINITION(name, type)\
void ANN_CONCAT(name,_init)(name* pVector, uint32_t initialCapacity, const ketl_allocator* pAllocator){\
    *pVector = (name){\
        .pAllocator = pAllocator,\
        .pData = ketl_alloc(pAllocator, sizeof(*pVector->pData) * initialCapacity),\
        .size = 0,\
        .capacity = initialCapacity};\
}\
void ANN_CONCAT(name,_deinit)(name* pVector){\
    ketl_free(pVector->pAllocator, pVector->pData);\
}\
void ANN_CONCAT(name,_clear)(name* pVector){\
    pVector->size = 0;\
}\
void ANN_CONCAT(name,_resize)(name* pVector, uint32_t newSize){\
    pVector->size = newSize;\
    ANN_CONCAT(name,_reserve)(pVector, newSize);\
}\
void ANN_CONCAT(name,_reserve)(name* pVector, uint32_t newCapacity){\
    uint32_t currentCapacity = pVector->capacity;\
    while (newCapacity > currentCapacity) {\
        currentCapacity = (uint32_t)(currentCapacity << 1);\
    }\
    if (currentCapacity != pVector->capacity) {\
        pVector->capacity = currentCapacity;\
        pVector->pData = ketl_realloc(pVector->pAllocator, pVector->pData, sizeof(*pVector->pData) * currentCapacity);\
    }\
}\
type* ANN_CONCAT(name,_push_back_copy)(name* pVector, type value){\
    type* pData = pVector->pData;\
    uint32_t index = pVector->size++;\
    uint32_t capacity = pVector->capacity;\
    if (index >= capacity) {\
        uint32_t newCapacity = pVector->capacity = (uint32_t)(capacity << 1);\
        pData = pVector->pData = ketl_realloc(pVector->pAllocator, pVector->pData, sizeof(*pVector->pData) * newCapacity);\
    }\
    pData += index;\
    *pData = value;\
    return pData;\
}\
type* ANN_CONCAT(name,_push_back_ref)(name* pVector, type const* pValue){\
    type* pData = pVector->pData;\
    uint32_t index = pVector->size++;\
    uint32_t capacity = pVector->capacity;\
    if (index >= capacity) {\
        uint32_t newCapacity = pVector->capacity = (uint32_t)(capacity << 1);\
        pData = pVector->pData = ketl_realloc(pVector->pAllocator, pVector->pData, sizeof(*pVector->pData) * newCapacity);\
    }\
    pData += index;\
    *pData = *pValue;\
    return pData;\
}\
type* ANN_CONCAT(name,_push_back_ref_n)(name* pVector, type const* pValues, uint32_t size){\
    if (size == 0) {\
        return pVector->pData + pVector->size;\
    }\
    type* pData = pVector->pData;\
    uint32_t index = pVector->size;\
    uint32_t newSize = pVector->size += size;\
    uint32_t capacity = pVector->capacity;\
    while (newSize > capacity) {\
        capacity = (uint32_t)(capacity << 1);\
    }\
    if (pVector->capacity != capacity) {\
        pVector->capacity = capacity;\
        pData = pVector->pData = ketl_realloc(pVector->pAllocator, pVector->pData, sizeof(*pVector->pData) * capacity);\
    }\
    pData += index;\
    ketl_memcpy(pData, pValues, sizeof(*pVector->pData) * size);\
    return pData;\
}\


#endif // ketl_containers_vector_h
