//🫖ketl
#ifndef ketl_containers_vector_h
#define ketl_containers_vector_h

#include "ketl/utils.h"

#include <stdlib.h>

#define KETL_VECTOR_DECLARATION(type) KETL_NAMED_VECTOR_DECLARATION(type, type)
#define KETL_NAMED_VECTOR_DECLARATION(name, type)\
KETL_DEFINE(KETL_CONCAT(ketl_vector_,name)) {\
	type* pData;\
	uint32_t size;\
	uint32_t capacity;\
};\
void KETL_CONCAT(ketl_vector_,name,_init)(KETL_CONCAT(ketl_vector_,name)* pVector, uint32_t initialCapacity);\
void KETL_CONCAT(ketl_vector_,name,_destroy)(KETL_CONCAT(ketl_vector_,name)* pVector);\
void KETL_CONCAT(ketl_vector_,name,_resize)(KETL_CONCAT(ketl_vector_,name)* pVector, uint32_t newSize);\
void KETL_CONCAT(ketl_vector_,name,_reserve)(KETL_CONCAT(ketl_vector_,name)* pVector, uint32_t newCapacity);\
type* KETL_CONCAT(ketl_vector_,name,_push_back_copy)(KETL_CONCAT(ketl_vector_,name)* pVector, type value);\
type* KETL_CONCAT(ketl_vector_,name,_push_back_ref)(KETL_CONCAT(ketl_vector_,name)* pVector, const type* pValue);\
type* KETL_CONCAT(ketl_vector_,name,_push_back_ref_n)(KETL_CONCAT(ketl_vector_,name)* pVector, const type* pValues, uint32_t size);\

#define KETL_VECTOR_DEFINITION(type) KETL_NAMED_VECTOR_DEFINITION(type, type)
#define KETL_NAMED_VECTOR_DEFINITION(name, type)\
void KETL_CONCAT(ketl_vector_,name,_init)(KETL_CONCAT(ketl_vector_,name)* pVector, uint32_t initialCapacity){\
    *pVector = (KETL_CONCAT(ketl_vector_,name)){\
        .pData = malloc(sizeof(type) * initialCapacity),\
        .size = 0,\
        .capacity = initialCapacity};\
}\
void KETL_CONCAT(ketl_vector_,name,_destroy)(KETL_CONCAT(ketl_vector_,name)* pVector){\
    free(pVector->pData);\
}\
void KETL_CONCAT(ketl_vector_,name,_resize)(KETL_CONCAT(ketl_vector_,name)* pVector, uint32_t newSize){\
    pVector->size = newSize;\
    KETL_CONCAT(ketl_vector_,name,_reserve)(pVector, newSize);\
}\
void KETL_CONCAT(ketl_vector_,name,_reserve)(KETL_CONCAT(ketl_vector_,name)* pVector, uint32_t newCapacity){\
    uint32_t currentCapacity = pVector->capacity;\
    while (newCapacity > currentCapacity) {\
        currentCapacity = (uint32_t)(currentCapacity << 1);\
    }\
    pVector->pData = realloc(pVector->pData, sizeof(type) * currentCapacity);\
}\
type* KETL_CONCAT(ketl_vector_,name,_push_back_copy)(KETL_CONCAT(ketl_vector_,name)* pVector, type value){\
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
type* KETL_CONCAT(ketl_vector_,name,_push_back_ref)(KETL_CONCAT(ketl_vector_,name)* pVector, const type* pValue){\
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
type* KETL_CONCAT(ketl_vector_,name,_push_back_ref_n)(KETL_CONCAT(ketl_vector_,name)* pVector, const type* pValues, uint32_t size){\
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