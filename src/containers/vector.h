//🫖ketl
#ifndef ketl_containers_vector_h
#define ketl_containers_vector_h

#include "ketl/utils.h"

#include <stdlib.h>

#define KETL_VECTOR_DECLARATION(type) KETL_NAMED_VECTOR_DECLARATION(KETL_CONCAT(type,_vector), type)
#define KETL_NAMED_VECTOR_DECLARATION(name, type)\
KETL_DEFINE(name) {\
	type* pData;\
	uint32_t size;\
	uint32_t capacity;\
};\
void KETL_CONCAT(name,_init)(name* pVector, uint32_t initialCapacity);\
void KETL_CONCAT(name,_destroy)(name* pVector);\
void KETL_CONCAT(name,_resize)(name* pVector, uint32_t newSize);\
void KETL_CONCAT(name,_reserve)(name* pVector, uint32_t newCapacity);\
type* KETL_CONCAT(name,_push_back_copy)(name* pVector, type value);\
type* KETL_CONCAT(name,_push_back_ref)(name* pVector, const type* pValue);\
type* KETL_CONCAT(name,_push_back_ref_n)(name* pVector, const type* pValues, uint32_t size);\

#define KETL_VECTOR_DEFINITION(type) KETL_NAMED_VECTOR_DEFINITION(KETL_CONCAT(type,_vector), type)
#define KETL_NAMED_VECTOR_DEFINITION(name, type)\
void KETL_CONCAT(name,_init)(name* pVector, uint32_t initialCapacity){\
    *pVector = (name){\
        .pData = malloc(sizeof(type) * initialCapacity),\
        .size = 0,\
        .capacity = initialCapacity};\
}\
void KETL_CONCAT(name,_destroy)(name* pVector){\
    free(pVector->pData);\
}\
void KETL_CONCAT(name,_resize)(name* pVector, uint32_t newSize){\
    pVector->size = newSize;\
    KETL_CONCAT(name,_reserve)(pVector, newSize);\
}\
void KETL_CONCAT(name,_reserve)(name* pVector, uint32_t newCapacity){\
    uint32_t currentCapacity = pVector->capacity;\
    while (newCapacity > currentCapacity) {\
        currentCapacity = (uint32_t)(currentCapacity << 1);\
    }\
    pVector->pData = realloc(pVector->pData, sizeof(type) * currentCapacity);\
}\
type* KETL_CONCAT(name,_push_back_copy)(name* pVector, type value){\
    type* pData = pVector->pData;\
    uint32_t index = pVector->size++;\
    uint32_t capacity = pVector->capacity;\
    if (index >= capacity) {\
        uint32_t newCapacity = pVector->capacity = (uint32_t)(capacity << 1);\
        pData = pVector->pData = realloc(pVector->pData, sizeof(type) * newCapacity);\
    }\
    pData += index;\
    *pData = value;\
    return pData;\
}\
type* KETL_CONCAT(name,_push_back_ref)(name* pVector, const type* pValue){\
    type* pData = pVector->pData;\
    uint32_t index = pVector->size++;\
    uint32_t capacity = pVector->capacity;\
    if (index >= capacity) {\
        uint32_t newCapacity = pVector->capacity = (uint32_t)(capacity << 1);\
        pData = pVector->pData = realloc(pVector->pData, sizeof(type) * newCapacity);\
    }\
    pData += index;\
    *pData = *pValue;\
    return pData;\
}\
type* KETL_CONCAT(name,_push_back_ref_n)(name* pVector, const type* pValues, uint32_t size){\
    if (size == 0) {\
        return NULL;\
    }\
    type* pData = pVector->pData;\
    uint32_t index = pVector->size;\
    uint32_t newSize = pVector->size += size;\
    uint32_t capacity = pVector->capacity;\
    while (newSize > capacity) {\
        capacity = pVector->capacity = (uint32_t)(capacity << 1);\
    }\
    pVector->capacity = capacity;\
    pData = pVector->pData = realloc(pVector->pData, sizeof(type) * capacity);\
    pData += index;\
    memcpy(pData, pValues, sizeof(type) * size);\
    return pData;\
}\


#endif // ketl_containers_vector_h