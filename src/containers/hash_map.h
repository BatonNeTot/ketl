//🫖ketl
#ifndef ketl_containers_hash_map_h
#define ketl_containers_hash_map_h

#include "ketl/utils.h"

#include "common.h"

#include "memory_impl.h"

#define KETL_HASH_MAP_DECLARATION(name, kType, vType)\
KETL_DEFINE(KETL_CONCAT(name,_bucket)) {\
    KETL_CONCAT(name,_bucket)* pNext;\
	uint64_t hash;\
	kType key;\
	vType value;\
};\
KETL_DEFINE(name) {\
    const ketl_allocator* pAllocator;\
	KETL_CONCAT(name,_bucket)** ppBuckets;\
	KETL_CONCAT(name,_bucket)* pFreeBucket;\
	uint32_t size;\
	uint32_t capacityIndex;\
};\
void KETL_CONCAT(name,_init)(name* pMap, const ketl_allocator* pAllocator);\
void KETL_CONCAT(name,_deinit)(name* pMap);\
KETL_CONCAT(name,_bucket)* KETL_CONCAT(name,_get_or_null)(name* pMap, kType key);\
KETL_CONCAT(name,_bucket)* KETL_CONCAT(name,_get_or_insert_copy)(name* pMap, kType key, vType value);\
void KETL_CONCAT(name,_erase)(name* pMap, KETL_CONCAT(name,_bucket)* pBucket);\

#define KETL_HASH_MAP_DEFINITION(name, kType, vType, kHash, kEqual)\
void KETL_CONCAT(name,_init)(name* pMap, const ketl_allocator* pAllocator) {\
    const uint32_t initialCapacityIndex = 0;\
    uint32_t initialCapacity = ketl_prime_capacities[initialCapacityIndex];\
    \
    const uint32_t arraySize = sizeof(KETL_CONCAT(name,_bucket)*) * initialCapacity;\
    const uint32_t alignedBucketsOffset = KETL_ALIGN_FORWARD(arraySize, _Alignof(KETL_CONCAT(name,_bucket)));\
    const uint32_t totalAllocateSize = alignedBucketsOffset + sizeof(KETL_CONCAT(name,_bucket)) * initialCapacity;\
    \
    void* bucketsAlloc = ketl_alloc(pAllocator, totalAllocateSize);\
    KETL_CONCAT(name,_bucket)** ppBuckets = bucketsAlloc;\
    KETL_CONCAT(name,_bucket)* pBucketsBuffer = (void*)((char*)bucketsAlloc + alignedBucketsOffset);\
    \
    *pMap = (name){\
        .pAllocator = pAllocator,\
        .ppBuckets = ppBuckets,\
        .pFreeBucket = pBucketsBuffer,\
        .size = 0,\
        .capacityIndex = initialCapacityIndex};\
    ketl_memset(ppBuckets, 0, arraySize);\
    \
    --initialCapacity;\
    for (uint32_t i = 0u; i < initialCapacity; ++i) {\
        pBucketsBuffer[i].pNext = pBucketsBuffer + i + 1;\
    }\
    pBucketsBuffer[initialCapacity].pNext = NULL;\
}\
void KETL_CONCAT(name,_deinit)(name* pMap) {\
    ketl_free(pMap->pAllocator, pMap->ppBuckets);\
}\
KETL_CONCAT(name,_bucket)* KETL_CONCAT(name,_get_or_null)(name* pMap, kType key) {\
    uint32_t capacity = ketl_prime_capacities[pMap->capacityIndex];\
	KETL_CONCAT(name,_bucket)** ppBuckets = pMap->ppBuckets;\
	uint64_t hash = kHash(key);\
	uint64_t index = hash % capacity;\
	KETL_CONCAT(name,_bucket)* pBucket = ppBuckets[index];\
\
	while (pBucket) {\
		if (pBucket->hash == hash && kEqual(pBucket->key, key)) {\
			return pBucket;\
		}\
\
		pBucket = pBucket->pNext;\
	}\
    return NULL;\
}\
KETL_CONCAT(name,_bucket)* KETL_CONCAT(name,_get_or_insert_copy)(name* pMap, kType key, vType value) {\
    uint32_t capacity = ketl_prime_capacities[pMap->capacityIndex];\
	KETL_CONCAT(name,_bucket)** ppBuckets = pMap->ppBuckets;\
	uint64_t hash = kHash(key);\
	uint64_t index = hash % capacity;\
	KETL_CONCAT(name,_bucket)* pBucket = ppBuckets[index];\
\
	while (pBucket) {\
		if (pBucket->hash == hash && kEqual(pBucket->key, key)) {\
			return pBucket;\
		}\
\
		pBucket = pBucket->pNext;\
	}\
\
	uint32_t size = ++pMap->size;\
	if (size > capacity) {\
		uint64_t newCapacityIndex = pMap->capacityIndex + 1;\
		if (KETL_PRIME_CAPACITIES_TOTAL <= newCapacityIndex) {\
			/* TODO error fatal */\
			return NULL;\
		}\
		uint32_t newCapacity = ketl_prime_capacities[pMap->capacityIndex = newCapacityIndex];\
		const uint32_t arraySize = sizeof(KETL_CONCAT(name,_bucket)*) * newCapacity;\
        const uint32_t alignedBucketsOffset = KETL_ALIGN_FORWARD(arraySize, _Alignof(KETL_CONCAT(name,_bucket)));\
        const uint32_t totalAllocateSize = alignedBucketsOffset + sizeof(KETL_CONCAT(name,_bucket)) * newCapacity;\
        \
        void* bucketsAlloc = ketl_alloc(pMap->pAllocator, totalAllocateSize);\
        KETL_CONCAT(name,_bucket)** ppNewBuckets = bucketsAlloc;\
        KETL_CONCAT(name,_bucket)* pBucketsBuffer = (void*)((char*)bucketsAlloc + alignedBucketsOffset);\
        pMap->ppBuckets = ppNewBuckets;\
\
        ketl_memset(ppNewBuckets, 0, arraySize);\
        uint32_t freeIndex = 0u;\
\
		for (uint32_t i = 0u; i < capacity; ++i) {\
			pBucket = ppBuckets[i];\
			while (pBucket) {\
				KETL_CONCAT(name,_bucket)* pNext = pBucket->pNext;\
\
                KETL_CONCAT(name,_bucket)* pNewBucket = pBucketsBuffer + freeIndex++;\
                *pNewBucket = *pBucket;\
\
				uint64_t newIndex = pNewBucket->hash % newCapacity;\
				pNewBucket->pNext = ppNewBuckets[newIndex];\
				ppNewBuckets[newIndex] = pNewBucket;\
\
				pBucket = pNext;\
			}\
		}\
		ketl_free(pMap->pAllocator, ppBuckets);\
		index = hash % newCapacity;\
		ppBuckets = ppNewBuckets;\
\
        \
        pMap->pFreeBucket = pBucketsBuffer + freeIndex;\
        --newCapacity;\
        for (; freeIndex < newCapacity; ++freeIndex) {\
            pBucketsBuffer[freeIndex].pNext = pBucketsBuffer + freeIndex + 1;\
        }\
        pBucketsBuffer[newCapacity].pNext = NULL;\
	}\
\
	pBucket = pMap->pFreeBucket;\
    pMap->pFreeBucket = pBucket->pNext;\
\
	pBucket->key = key;\
	pBucket->value = value;\
	pBucket->hash = hash;\
	pBucket->pNext = ppBuckets[index];\
\
	ppBuckets[index] = pBucket;\
	return pBucket;\
}\
void KETL_CONCAT(name,_erase)(name* pMap, KETL_CONCAT(name,_bucket)* pBucket) {\
    uint32_t capacity = ketl_prime_capacities[pMap->capacityIndex];\
	KETL_CONCAT(name,_bucket)** ppBuckets = pMap->ppBuckets;\
	uint64_t hash = kHash(pBucket->key);\
	uint64_t index = hash % capacity;\
	KETL_CONCAT(name,_bucket)* pHeadBucket = ppBuckets[index];\
\
    if (pHeadBucket == pBucket) {\
        ppBuckets[index] = pBucket->pNext;\
        return;\
    }\
\
    while (pHeadBucket) {\
        if (pHeadBucket->pNext == pBucket) {\
            pHeadBucket->pNext = pBucket->pNext;\
            return;\
        }\
        pHeadBucket = pHeadBucket->pNext;\
    }\
}\

#define KETL_HASH_MAP_FOREACH(name, pMap, runnable)\
do {\
    name* __pMap = (pMap);\
    uint32_t __capacity = ketl_prime_capacities[__pMap->capacityIndex];\
    KETL_CONCAT(name,_bucket)** __ppBuckets = __pMap->ppBuckets;\
	for (uint32_t __i = 0u; __i < __capacity; ++__i) {\
        KETL_CONCAT(name,_bucket)* __pBucket = __ppBuckets[__i];\
        while (__pBucket) {\
            runnable\
            __pBucket = __pBucket->pNext;\
        }\
    }\
} while (false)

#endif // ketl_containers_hash_map_h
