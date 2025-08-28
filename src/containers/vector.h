//🫖ketl
#ifndef ketl_containers_vector_h
#define ketl_containers_vector_h

#include "ketl/utils.h"

#include "memory_impl.h"

#define KETL_VECTOR_DECLARATION(name, type)\
ANN_DEFINE(name) {\
    const ketl_allocator* p_allocator;\
	type* p_data;\
	uint32_t size;\
	uint32_t capacity;\
};\
void ANN_CONCAT(name,_init)(name* pVector, uint32_t initialCapacity, const ketl_allocator* p_allocator);\
void ANN_CONCAT(name,_deinit)(name* pVector);\
void ANN_CONCAT(name,_clear)(name* pVector);\
void ANN_CONCAT(name,_resize)(name* pVector, uint32_t newSize);\
void ANN_CONCAT(name,_reserve)(name* pVector, uint32_t newCapacity);\
type* ANN_CONCAT(name,_push_back_copy)(name* pVector, type value);\
type* ANN_CONCAT(name,_push_back_ref)(name* pVector, type const* pValue);\
type* ANN_CONCAT(name,_push_back_ref_n)(name* pVector, type const* pValues, uint32_t size);\

#define KETL_VECTOR_DEFINITION(name, type)\
void ANN_CONCAT(name,_init)(name* pVector, uint32_t initialCapacity, const ketl_allocator* p_allocator){\
    *pVector = (name){\
        .p_allocator = p_allocator,\
        .p_data = ketl_alloc(p_allocator, sizeof(*pVector->p_data) * initialCapacity),\
        .size = 0,\
        .capacity = initialCapacity};\
}\
void ANN_CONCAT(name,_deinit)(name* pVector){\
    ketl_free(pVector->p_allocator, pVector->p_data);\
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
        pVector->p_data = ketl_realloc(pVector->p_allocator, pVector->p_data, sizeof(*pVector->p_data) * currentCapacity);\
    }\
}\
type* ANN_CONCAT(name,_push_back_copy)(name* pVector, type value){\
    type* p_data = pVector->p_data;\
    uint32_t index = pVector->size++;\
    uint32_t capacity = pVector->capacity;\
    if (index >= capacity) {\
        uint32_t newCapacity = pVector->capacity = (uint32_t)(capacity << 1);\
        p_data = pVector->p_data = ketl_realloc(pVector->p_allocator, pVector->p_data, sizeof(*pVector->p_data) * newCapacity);\
    }\
    p_data += index;\
    *p_data = value;\
    return p_data;\
}\
type* ANN_CONCAT(name,_push_back_ref)(name* pVector, type const* pValue){\
    type* p_data = pVector->p_data;\
    uint32_t index = pVector->size++;\
    uint32_t capacity = pVector->capacity;\
    if (index >= capacity) {\
        uint32_t newCapacity = pVector->capacity = (uint32_t)(capacity << 1);\
        p_data = pVector->p_data = ketl_realloc(pVector->p_allocator, pVector->p_data, sizeof(*pVector->p_data) * newCapacity);\
    }\
    p_data += index;\
    *p_data = *pValue;\
    return p_data;\
}\
type* ANN_CONCAT(name,_push_back_ref_n)(name* pVector, type const* pValues, uint32_t size){\
    if (size == 0) {\
        return pVector->p_data + pVector->size;\
    }\
    type* p_data = pVector->p_data;\
    uint32_t index = pVector->size;\
    uint32_t newSize = pVector->size += size;\
    uint32_t capacity = pVector->capacity;\
    while (newSize > capacity) {\
        capacity = (uint32_t)(capacity << 1);\
    }\
    if (pVector->capacity != capacity) {\
        pVector->capacity = capacity;\
        p_data = pVector->p_data = ketl_realloc(pVector->p_allocator, pVector->p_data, sizeof(*pVector->p_data) * capacity);\
    }\
    p_data += index;\
    ketl_memcpy(p_data, pValues, sizeof(*pVector->p_data) * size);\
    return p_data;\
}\


#endif // ketl_containers_vector_h
