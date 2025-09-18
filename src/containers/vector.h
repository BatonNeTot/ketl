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
void ANN_CONCAT(name,_init)(name* p_vector, uint32_t initial_capacity, const ketl_allocator* p_allocator);\
void ANN_CONCAT(name,_deinit)(name* p_vector);\
void ANN_CONCAT(name,_clear)(name* p_vector);\
void ANN_CONCAT(name,_resize)(name* p_vector, uint32_t new_size);\
void ANN_CONCAT(name,_reserve)(name* p_vector, uint32_t new_capacity);\
type* ANN_CONCAT(name,_push_back_copy)(name* p_vector, type value);\
type* ANN_CONCAT(name,_push_back_ref)(name* p_vector, type const* p_value);\
type* ANN_CONCAT(name,_push_back_ref_n)(name* p_vector, type const* p_values, uint32_t size);\

#define KETL_VECTOR_DEFINITION(name, type)\
void ANN_CONCAT(name,_init)(name* p_vector, uint32_t initial_capacity, const ketl_allocator* p_allocator){\
    *p_vector = (name){\
        .p_allocator = p_allocator,\
        .p_data = ketl_alloc(p_allocator, sizeof(*p_vector->p_data) * initial_capacity),\
        .size = 0,\
        .capacity = initial_capacity};\
}\
void ANN_CONCAT(name,_deinit)(name* p_vector){\
    ketl_free(p_vector->p_allocator, p_vector->p_data);\
}\
void ANN_CONCAT(name,_clear)(name* p_vector){\
    p_vector->size = 0;\
}\
void ANN_CONCAT(name,_resize)(name* p_vector, uint32_t new_size){\
    p_vector->size = new_size;\
    ANN_CONCAT(name,_reserve)(p_vector, new_size);\
}\
void ANN_CONCAT(name,_reserve)(name* p_vector, uint32_t new_capacity){\
    uint32_t current_capacity = p_vector->capacity;\
    while (new_capacity > current_capacity) {\
        current_capacity = (uint32_t)(current_capacity << 1);\
    }\
    if (current_capacity != p_vector->capacity) {\
        p_vector->capacity = current_capacity;\
        p_vector->p_data = ketl_realloc(p_vector->p_allocator, p_vector->p_data, sizeof(*p_vector->p_data) * current_capacity);\
    }\
}\
type* ANN_CONCAT(name,_push_back_copy)(name* p_vector, type value){\
    type* p_data = p_vector->p_data;\
    uint32_t index = p_vector->size++;\
    uint32_t capacity = p_vector->capacity;\
    if (index >= capacity) {\
        uint32_t new_capacity = p_vector->capacity = (uint32_t)(capacity << 1);\
        p_data = p_vector->p_data = ketl_realloc(p_vector->p_allocator, p_vector->p_data, sizeof(*p_vector->p_data) * new_capacity);\
    }\
    p_data += index;\
    *p_data = value;\
    return p_data;\
}\
type* ANN_CONCAT(name,_push_back_ref)(name* p_vector, type const* p_value){\
    type* p_data = p_vector->p_data;\
    uint32_t index = p_vector->size++;\
    uint32_t capacity = p_vector->capacity;\
    if (index >= capacity) {\
        uint32_t new_capacity = p_vector->capacity = (uint32_t)(capacity << 1);\
        p_data = p_vector->p_data = ketl_realloc(p_vector->p_allocator, p_vector->p_data, sizeof(*p_vector->p_data) * new_capacity);\
    }\
    p_data += index;\
    *p_data = *p_value;\
    return p_data;\
}\
type* ANN_CONCAT(name,_push_back_ref_n)(name* p_vector, type const* p_values, uint32_t size){\
    if (size == 0) {\
        return p_vector->p_data + p_vector->size;\
    }\
    type* p_data = p_vector->p_data;\
    uint32_t index = p_vector->size;\
    uint32_t new_size = p_vector->size += size;\
    uint32_t capacity = p_vector->capacity;\
    while (new_size > capacity) {\
        capacity = (uint32_t)(capacity << 1);\
    }\
    if (p_vector->capacity != capacity) {\
        p_vector->capacity = capacity;\
        p_data = p_vector->p_data = ketl_realloc(p_vector->p_allocator, p_vector->p_data, sizeof(*p_vector->p_data) * capacity);\
    }\
    p_data += index;\
    ketl_memcpy(p_data, p_values, sizeof(*p_vector->p_data) * size);\
    return p_data;\
}\


#endif // ketl_containers_vector_h
