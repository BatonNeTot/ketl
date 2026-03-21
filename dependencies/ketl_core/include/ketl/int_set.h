//🍲ketl
#ifndef int_set_h
#define int_set_h

#include "ketl/object_pool.h"
#include "ketl/utils.h"

typedef uint64_t KETLIntSetElement;

KETL_FORWARD(KETLIntSetBucketBase);

KETL_DEFINE(KETLIntSet) {
	KETLObjectPool bucketPool;
	KETLIntSetBucketBase** buckets;
	uint64_t size;
	uint64_t capacityIndex;
};

void ketlInitIntSet(KETLIntSet* set, size_t poolSize);

void ketlDeinitIntSet(KETLIntSet* set);

void ketlIntSetPut(KETLIntSet* set, KETLIntSetElement element);

inline uint64_t ketlIntSetGetSize(KETLIntSet* set) {
	return set->size;
}

void ketlIntSetReset(KETLIntSet* set);

KETL_DEFINE(KETLIntSetIterator) {
	KETLIntSet* set;
	uint64_t currentIndex;
	KETLIntSetBucketBase* currentBucket;
};

void ketlInitIntSetIterator(KETLIntSetIterator* iterator, KETLIntSet* set);

bool ketlIntSetIteratorHasNext(KETLIntSetIterator* iterator);

void ketlIntSetIteratorGet(KETLIntSetIterator* iterator, KETLIntSetElement* pElement);

void ketlIntSetIteratorNext(KETLIntSetIterator* iterator);

void ketlIntSetIteratorRemove(KETLIntSetIterator* iterator);

#endif /*int_set_h*/
