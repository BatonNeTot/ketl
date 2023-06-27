//🍲ketl
#include "ketl/int_map.h"

#include <stdlib.h>
#include <string.h>

static const uint64_t primeCapacities[] =
{
  0,          3,          11,         23,         53,
  97,         193,        389,        769,        1543,
  3079,       6151,       12289,      24593,      49157,
  98317,      196613,     393241,     786433,     1572869,
  3145739,    6291469,    12582917,   25165843,   50331653,
  100663319,  201326611,  402653189,  805306457,  1610612741,
  3221225473, 4294967291
};

const uint64_t TOTAL_PRIME_CAPACITIES = sizeof(primeCapacities) / sizeof(primeCapacities[0]);

struct KETLIntMapBucketBase {
	KETLIntMapKey key;
	KETLIntMapBucketBase* next;
};

void ketlInitIntMap(KETLIntMap* map, size_t objectSize, size_t poolSize) {
	// TODO align
	const size_t bucketSize = sizeof(KETLIntMapBucketBase) + objectSize;
	ketlInitObjectPool(&map->bucketPool, bucketSize, poolSize);
	map->size = 0;
	map->capacityIndex = 0;
	map->buckets = NULL;
}

void ketlDeinitIntMap(KETLIntMap* map) {
	ketlDeinitObjectPool(&map->bucketPool);
	free(map->buckets);
}

void* ketlIntMapPut(KETLIntMap* map, KETLIntMapKey key) {
	if (map->size == 0) {
		map->capacityIndex = 1;
		map->size = 1;

		const uint64_t capacity = primeCapacities[1];
		uint64_t arraySize = sizeof(KETLIntMapBucketBase*) * capacity;
		KETLIntMapBucketBase** buckets = map->buckets = malloc(arraySize);
		memset(buckets, 0, arraySize);

		KETLIntMapBucketBase* bucket = ketlGetFreeObjectFromPool(&map->bucketPool);
		bucket->key = key;
		bucket->next = NULL;

		buckets[key % capacity] = bucket;
		return bucket + 1;
	}
	else {
		uint64_t capacity = primeCapacities[map->capacityIndex];
		KETLIntMapBucketBase** buckets = map->buckets;
		const uint64_t index = key % capacity;
		KETLIntMapBucketBase* bucket = buckets[index];

		while (bucket) {
			if (bucket->key == key) {
				return bucket + 1;
			}

			bucket->next;
		}

		uint64_t size = ++map->size;
		if (size > capacity) {
			capacity = primeCapacities[++map->capacityIndex];
			map->buckets = buckets = realloc(buckets, capacity);
			memset(buckets + (size - 1), 0, sizeof(KETLIntMapBucketBase*) * (capacity - (size - 1)));
		}

		bucket = ketlGetFreeObjectFromPool(&map->bucketPool);
		bucket->key = key;
		bucket->next = buckets[index];

		buckets[index] = bucket;
		return bucket + 1;
	}
}