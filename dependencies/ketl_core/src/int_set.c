//🍲ketl
#include "ketl/int_set.h"

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

static const uint64_t TOTAL_PRIME_CAPACITIES = sizeof(primeCapacities) / sizeof(primeCapacities[0]);

KETL_DEFINE(KETLIntSetBucketBase) {
	KETLIntSetElement element;
	KETLIntSetBucketBase* next;
};

void ketlInitIntSet(KETLIntSet* set, size_t poolSize) {
	// TODO align
	const size_t bucketSize = sizeof(KETLIntSetBucketBase);
	ketlInitObjectPool(&set->bucketPool, bucketSize, poolSize);
	set->capacityIndex = 0;
	uint64_t capacity = primeCapacities[0];
	set->size = 0;
	uint64_t arraySize = sizeof(KETLIntSetBucketBase*) * capacity;
	KETLIntSetBucketBase** buckets = set->buckets = malloc(arraySize);
	// TODO use custom memset
	memset(buckets, 0, arraySize);
}

void ketlDeinitIntSet(KETLIntSet* set) {
	ketlDeinitObjectPool(&set->bucketPool);
	free(set->buckets);
}

void ketlIntSetPut(KETLIntSet* set, KETLIntSetElement element) {
	uint64_t capacity = primeCapacities[set->capacityIndex];
	KETLIntSetBucketBase** buckets = set->buckets;
	uint64_t index = element % capacity;
	KETLIntSetBucketBase* bucket = buckets[index];

	while (bucket) {
		if (bucket->element == element) {
			return;
		}

		bucket = bucket->next;
	}

	uint64_t size = ++set->size;
	if (size > capacity) {
		uint64_t newCapacityIndex = set->capacityIndex + 1;
		if (TOTAL_PRIME_CAPACITIES <= newCapacityIndex) {
			// TODO error
			return;
		}
		uint64_t newCapacity = primeCapacities[set->capacityIndex = newCapacityIndex];
		uint64_t arraySize = sizeof(KETLIntSetBucketBase*) * newCapacity;
		KETLIntSetBucketBase** newBuckets = set->buckets = malloc(arraySize);
		// TODO use custom memset
		memset(newBuckets, 0, arraySize);
		for (uint64_t i = 0; i < capacity; ++i) {
			bucket = buckets[i];
			while (bucket) {
				KETLIntSetBucketBase* next = bucket->next;
				uint64_t newIndex = bucket->element % newCapacity;
				bucket->next = newBuckets[newIndex];
				newBuckets[newIndex] = bucket;
				bucket = next;
			}
		}
		free(buckets);
		index = element % newCapacity;
		buckets = newBuckets;
	}

	bucket = ketlGetFreeObjectFromPool(&set->bucketPool);

	bucket->element = element;
	bucket->next = buckets[index];
	buckets[index] = bucket;
}

void ketlIntSetReset(KETLIntSet* set) {
	ketlResetPool(&set->bucketPool);
	set->size = 0;
	uint64_t capacity = primeCapacities[set->capacityIndex];
	uint64_t arraySize = sizeof(KETLIntSetBucketBase*) * capacity;
	// TODO use custom memset
	memset(set->buckets, 0, arraySize);
}


void ketlInitIntSetIterator(KETLIntSetIterator* iterator, KETLIntSet* set) {
	iterator->set = set;
	uint64_t i = 0;
	uint64_t capacity = primeCapacities[set->capacityIndex];
	KETLIntSetBucketBase** buckets = set->buckets;
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

bool ketlIntSetIteratorHasNext(KETLIntSetIterator* iterator) {
	return iterator->currentBucket != NULL;
}

void ketlIntSetIteratorGet(KETLIntSetIterator* iterator, KETLIntSetElement* pElement) {
	*pElement = iterator->currentBucket->element;
}

void ketlIntSetIteratorNext(KETLIntSetIterator* iterator) {
	KETLIntSetBucketBase* nextBucket = iterator->currentBucket->next;
	if (nextBucket) {
		iterator->currentBucket = nextBucket;
		return;
	}

	uint64_t i = iterator->currentIndex + 1;
	uint64_t capacity = primeCapacities[iterator->set->capacityIndex];
	KETLIntSetBucketBase** buckets = iterator->set->buckets;
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

void ketlIntSetIteratorRemove(KETLIntSetIterator* iterator) {
	--iterator->set->size;
	uint64_t i = iterator->currentIndex;
	KETLIntSetBucketBase** buckets = iterator->set->buckets;
	
	KETLIntSetBucketBase* parent = buckets[i];
	KETLIntSetBucketBase* currentBucket = iterator->currentBucket;
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
	uint64_t capacity = primeCapacities[iterator->set->capacityIndex];
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