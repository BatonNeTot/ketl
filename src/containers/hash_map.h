//🫖ketl
#ifndef ketl_containers_hash_map_h
#define ketl_containers_hash_map_h

#include "ketl/utils.h"

#include "common.h"

#include "memory_impl.h"

#define KETL_HASH_MAP_DECLARATION(name, k_type, v_type)\
ANN_DEFINE(ANN_CONCAT(name,_bucket)) {\
    ANN_CONCAT(name,_bucket)* p_next;\
	uint64_t hash;\
	k_type key;\
    v_type value;\
};\
ANN_DEFINE(name) {\
    const ketl_allocator* p_allocator;\
	ANN_CONCAT(name,_bucket)** pp_buckets;\
	ANN_CONCAT(name,_bucket)* p_free_bucket;\
	uint32_t size;\
	uint32_t capacity_index;\
};\
void ANN_CONCAT(name,_init)(name* p_map, const ketl_allocator* p_allocator);\
void ANN_CONCAT(name,_deinit)(name* p_map);\
ANN_CONCAT(name,_bucket)* ANN_CONCAT(name,_get_or_null)(name* p_map, k_type key);\
ANN_CONCAT(name,_bucket)* ANN_CONCAT(name,_get_or_insert_copy)(name* p_map, k_type key, v_type value);\
void ANN_CONCAT(name,_erase)(name* p_map, ANN_CONCAT(name,_bucket)* p_bucket);\
void ANN_CONCAT(name,_clear)(name* p_map);\

#define KETL_HASH_MAP_DEFINITION(name, k_type, v_type, k_hash, k_equal)\
void ANN_CONCAT(name,_init)(name* p_map, const ketl_allocator* p_allocator) {\
    const uint32_t initial_capacity_index = 0;\
    uint32_t initial_capacity = ketl_prime_capacities[initial_capacity_index];\
    \
    const uint32_t array_size = sizeof(ANN_CONCAT(name,_bucket)*) * initial_capacity;\
    const uint32_t aligned_buckets_offset = ANN_ALIGN_FORWARD(array_size, _Alignof(ANN_CONCAT(name,_bucket)));\
    const uint32_t total_allocate_size = aligned_buckets_offset + sizeof(ANN_CONCAT(name,_bucket)) * initial_capacity;\
    \
    void* buckets_alloc = ketl_alloc(p_allocator, total_allocate_size);\
    ANN_CONCAT(name,_bucket)** pp_buckets = buckets_alloc;\
    ANN_CONCAT(name,_bucket)* p_buckets_buffer = (void*)((char*)buckets_alloc + aligned_buckets_offset);\
    \
    *p_map = (name){\
        .p_allocator = p_allocator,\
        .pp_buckets = pp_buckets,\
        .p_free_bucket = p_buckets_buffer,\
        .size = 0,\
        .capacity_index = initial_capacity_index};\
    ketl_memset(pp_buckets, 0, array_size);\
    \
    --initial_capacity;\
    for (uint32_t i = 0u; i < initial_capacity; ++i) {\
        p_buckets_buffer[i].p_next = p_buckets_buffer + i + 1;\
    }\
    p_buckets_buffer[initial_capacity].p_next = NULL;\
}\
void ANN_CONCAT(name,_deinit)(name* p_map) {\
    ketl_free(p_map->p_allocator, p_map->pp_buckets);\
}\
ANN_CONCAT(name,_bucket)* ANN_CONCAT(name,_get_or_null)(name* p_map, k_type key) {\
    uint32_t capacity = ketl_prime_capacities[p_map->capacity_index];\
	ANN_CONCAT(name,_bucket)** pp_buckets = p_map->pp_buckets;\
	uint64_t hash = k_hash(key);\
	uint64_t index = hash % capacity;\
	ANN_CONCAT(name,_bucket)* p_bucket = pp_buckets[index];\
\
	while (p_bucket) {\
		if (p_bucket->hash == hash && k_equal(p_bucket->key, key)) {\
			return p_bucket;\
		}\
\
		p_bucket = p_bucket->p_next;\
	}\
    return NULL;\
}\
ANN_CONCAT(name,_bucket)* ANN_CONCAT(name,_get_or_insert_copy)(name* p_map, k_type key, v_type value) {\
    uint32_t capacity = ketl_prime_capacities[p_map->capacity_index];\
	ANN_CONCAT(name,_bucket)** pp_buckets = p_map->pp_buckets;\
	uint64_t hash = k_hash(key);\
	uint64_t index = hash % capacity;\
	ANN_CONCAT(name,_bucket)* p_bucket = pp_buckets[index];\
\
	while (p_bucket) {\
		if (p_bucket->hash == hash && k_equal(p_bucket->key, key)) {\
			return p_bucket;\
		}\
\
		p_bucket = p_bucket->p_next;\
	}\
\
	uint32_t size = ++p_map->size;\
	if (size > capacity) {\
		uint32_t new_capacity_index = p_map->capacity_index + 1;\
		if (KETL_PRIME_CAPACITIES_TOTAL <= new_capacity_index) {\
			/* TODO error fatal */\
			return NULL;\
		}\
		uint32_t new_capacity = ketl_prime_capacities[p_map->capacity_index = new_capacity_index];\
		const uint32_t array_size = sizeof(ANN_CONCAT(name,_bucket)*) * new_capacity;\
        const uint32_t aligned_buckets_offset = ANN_ALIGN_FORWARD(array_size, _Alignof(ANN_CONCAT(name,_bucket)));\
        const uint32_t total_allocate_size = aligned_buckets_offset + sizeof(ANN_CONCAT(name,_bucket)) * new_capacity;\
        \
        void* buckets_alloc = ketl_alloc(p_map->p_allocator, total_allocate_size);\
        ANN_CONCAT(name,_bucket)** pp_new_buckets = buckets_alloc;\
        ANN_CONCAT(name,_bucket)* p_buckets_buffer = (void*)((char*)buckets_alloc + aligned_buckets_offset);\
        p_map->pp_buckets = pp_new_buckets;\
\
        ketl_memset(pp_new_buckets, 0, array_size);\
        uint32_t free_index = 0u;\
\
		for (uint32_t i = 0u; i < capacity; ++i) {\
			p_bucket = pp_buckets[i];\
			while (p_bucket) {\
				ANN_CONCAT(name,_bucket)* p_next = p_bucket->p_next;\
\
                ANN_CONCAT(name,_bucket)* p_new_bucket = p_buckets_buffer + free_index++;\
                *p_new_bucket = *p_bucket;\
\
				uint64_t new_index = p_new_bucket->hash % new_capacity;\
				p_new_bucket->p_next = pp_new_buckets[new_index];\
				pp_new_buckets[new_index] = p_new_bucket;\
\
				p_bucket = p_next;\
			}\
		}\
		ketl_free(p_map->p_allocator, pp_buckets);\
		index = hash % new_capacity;\
		pp_buckets = pp_new_buckets;\
\
        \
        p_map->p_free_bucket = p_buckets_buffer + free_index;\
        --new_capacity;\
        for (; free_index < new_capacity; ++free_index) {\
            p_buckets_buffer[free_index].p_next = p_buckets_buffer + free_index + 1;\
        }\
        p_buckets_buffer[new_capacity].p_next = NULL;\
	}\
\
	p_bucket = p_map->p_free_bucket;\
    p_map->p_free_bucket = p_bucket->p_next;\
\
	p_bucket->key = key;\
	p_bucket->value = value;\
	p_bucket->hash = hash;\
	p_bucket->p_next = pp_buckets[index];\
\
	pp_buckets[index] = p_bucket;\
	return p_bucket;\
}\
void ANN_CONCAT(name,_erase)(name* p_map, ANN_CONCAT(name,_bucket)* p_bucket) {\
    uint32_t capacity = ketl_prime_capacities[p_map->capacity_index];\
	ANN_CONCAT(name,_bucket)** pp_buckets = p_map->pp_buckets;\
	uint64_t hash = k_hash(p_bucket->key);\
	uint64_t index = hash % capacity;\
	ANN_CONCAT(name,_bucket)* p_head_bucket = pp_buckets[index];\
\
    if (p_head_bucket == p_bucket) {\
        pp_buckets[index] = p_bucket->p_next;\
        p_bucket->p_next = p_map->p_free_bucket;\
        p_map->p_free_bucket = p_bucket;\
        --p_map->size;\
        return;\
    }\
\
    while (p_head_bucket) {\
        if (p_head_bucket->p_next == p_bucket) {\
            p_head_bucket->p_next = p_bucket->p_next;\
            p_bucket->p_next = p_map->p_free_bucket;\
            p_map->p_free_bucket = p_bucket;\
            --p_map->size;\
            return;\
        }\
        p_head_bucket = p_head_bucket->p_next;\
    }\
}\
void ANN_CONCAT(name,_clear)(name* p_map) {\
    uint32_t capacity = ketl_prime_capacities[p_map->capacity_index];\
    for (uint32_t i = 0u; i < capacity; ++i) {\
        ANN_CONCAT(name,_bucket)* p_bucket = p_map->pp_buckets[i];\
        while (p_bucket) {\
            ANN_CONCAT(name,_bucket)* p_next = p_bucket->p_next;\
    \
            p_bucket->p_next = p_map->p_free_bucket;\
            p_map->p_free_bucket = p_bucket;\
    \
            p_bucket = p_next;\
        }\
    }\
    p_map->size = 0;\
}\

#define KETL_HASH_MAP_FOREACH(name, p_map, runnable)\
do {\
    name* __p_map = (p_map);\
    uint32_t __capacity = ketl_prime_capacities[__p_map->capacity_index];\
    ANN_CONCAT(name,_bucket)** __pp_buckets = __p_map->pp_buckets;\
	for (uint32_t __i = 0u; __i < __capacity; ++__i) {\
        ANN_CONCAT(name,_bucket)* __p_bucket = __pp_buckets[__i];\
        while (__p_bucket) {\
            runnable\
            __p_bucket = __p_bucket->p_next;\
        }\
    }\
} while (false)

#endif // ketl_containers_hash_map_h
