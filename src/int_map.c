//🍲ketl
#include "ketl/int_map.h"

#include <stdlib.h>
#include <string.h>

static const uint64_t primeCapacities[] =
{
  3ULL, 11UL, 23ULL, 53ULL, 97ULL, 193ULL, 389ULL,
  769ULL, 1543ULL, 3079ULL, 6151ULL, 12289ULL,
  24593ULL, 49157ULL, 98317ULL, 196613ULL, 393241ULL,
  786433ULL, 1572869ULL, 3145739ULL, 6291469ULL,
  12582917ULL, 25165843ULL, 50331653ULL, 100663319ULL,
  201326611ULL, 402653189ULL, 805306457ULL,
  1610612741ULL, 3221225473ULL, 4294967291ULL
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
	map->capacityIndex = 0;
	uint64_t capacity = primeCapacities[0];
	map->size = 0;
	uint64_t arraySize = sizeof(KETLIntMapBucketBase*) * capacity;
	KETLIntMapBucketBase** buckets = map->buckets = malloc(arraySize);
	// TODO use custom memset
	memset(buckets, 0, arraySize);
}

void ketlDeinitIntMap(KETLIntMap* map) {
	ketlDeinitObjectPool(&map->bucketPool);
	free(map->buckets);
}

bool ketlIntMapGetOrCreate(KETLIntMap* map, KETLIntMapKey key, void* ppValue) {
	uint64_t capacity = primeCapacities[map->capacityIndex];
	KETLIntMapBucketBase** buckets = map->buckets;
	uint64_t index = key % capacity;
	KETLIntMapBucketBase* bucket = buckets[index];

	while (bucket) {
		if (bucket->key == key) {
			*(void**)(ppValue) = bucket + 1;
			return false;
		}

		bucket = bucket->next;
	}

	uint64_t size = ++map->size;
	if (size > capacity) {
		uint64_t newCapacity = primeCapacities[++map->capacityIndex];
		uint64_t arraySize = sizeof(KETLIntMapBucketBase*) * newCapacity;
		KETLIntMapBucketBase** newBuckets = map->buckets = malloc(arraySize);
		// TODO use custom memset
		memset(newBuckets, 0, arraySize);
		for (uint64_t i = 0; i < capacity; ++i) {
			bucket = buckets[i];
			while (bucket) {
				KETLIntMapBucketBase* next = bucket->next;
				uint64_t newIndex = bucket->key % newCapacity;
				bucket->next = newBuckets[newIndex];
				newBuckets[newIndex] = bucket;
				bucket = next;
			}
		}
		free(buckets);
		index = key % newCapacity;
		buckets = newBuckets;
	}

	bucket = ketlGetFreeObjectFromPool(&map->bucketPool);

	bucket->key = key;
	bucket->next = buckets[index];
	buckets[index] = bucket;

	*(void**)(ppValue) = bucket + 1;
	return true;
}

void ketlIntMapReset(KETLIntMap* map) {
	ketlResetPool(&map->bucketPool);
	map->size = 0;
	uint64_t capacity = primeCapacities[map->capacityIndex];
	uint64_t arraySize = sizeof(KETLIntMapBucketBase*) * capacity;
	// TODO use custom memset
	memset(map->buckets, 0, arraySize);
}


void ketlInitIntMapIterator(KETLIntMapIterator* iterator, KETLIntMap* map) {
	iterator->map = map;
	uint64_t i = 0;
	uint64_t capacity = primeCapacities[map->capacityIndex];
	KETLIntMapBucketBase** buckets = map->buckets;
	KETL_FOREVER{
		if (i >= capacity) {
			iterator->currentBucket = NULL;
			return;
		}

		if (buckets[i] != NULL) {
			iterator->currentIndex = i;
			iterator->currentBucket = buckets[i];
			return;
		}

		++i;
	}
}

bool ketlIntMapIteratorHasNext(KETLIntMapIterator* iterator) {
	return iterator->currentBucket != NULL;
}

void ketlIntMapIteratorGet(KETLIntMapIterator* iterator, KETLIntMapKey* pKey, void* ppValue) {
	*pKey = iterator->currentBucket->key;
	*(void**)(ppValue) = iterator->currentBucket + 1;
}

void ketlIntMapIteratorNext(KETLIntMapIterator* iterator) {
	KETLIntMapBucketBase* nextBucket = iterator->currentBucket->next;
	if (nextBucket) {
		iterator->currentBucket = nextBucket;
		return;
	}

	uint64_t i = iterator->currentIndex + 1;
	uint64_t capacity = primeCapacities[iterator->map->capacityIndex];
	KETLIntMapBucketBase** buckets = iterator->map->buckets;
	KETL_FOREVER{
		if (i >= capacity) {
			iterator->currentBucket = NULL;
			return;
		}

		if (buckets[i] != NULL) {
			iterator->currentIndex = i;
			iterator->currentBucket = buckets[i];
			return;
		}

		++i;
	}
}

void ketlIntMapIteratorRemove(KETLIntMapIterator* iterator) {
	--iterator->map->size;
	uint64_t i = iterator->currentIndex;
	KETLIntMapBucketBase** buckets = iterator->map->buckets;
	
	KETLIntMapBucketBase* parent = buckets[i];
	KETLIntMapBucketBase* currentBucket = iterator->currentBucket;
	if (parent == currentBucket) {
		buckets[i] = currentBucket = currentBucket->next;
	}
	else {
		while (parent->next != currentBucket) {
			parent = parent->next;
		}
		parent->next = currentBucket = currentBucket->next;
	}

	if (currentBucket != NULL) {
		iterator->currentBucket = currentBucket;
		return;
	}

	++i;
	uint64_t capacity = primeCapacities[iterator->map->capacityIndex];
	KETL_FOREVER{
		if (i >= capacity) {
			iterator->currentBucket = NULL;
			return;
		}

		if (buckets[i] != NULL) {
			iterator->currentIndex = i;
			iterator->currentBucket = buckets[i];
			return;
		}

		++i;
	}
}