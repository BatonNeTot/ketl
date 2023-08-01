//🍲ketl
#ifndef int_map_h
#define int_map_h

#include "ketl/object_pool.h"
#include "ketl/utils.h"

typedef uint64_t KETLIntMapKey;

KETL_FORWARD(KETLIntMapBucketBase);

KETL_DEFINE(KETLIntMap) {
	KETLObjectPool bucketPool;
	KETLIntMapBucketBase** buckets;
	uint64_t size;
	uint64_t capacityIndex;
};

void ketlInitIntMap(KETLIntMap* map, size_t objectSize, size_t poolSize);

void ketlDeinitIntMap(KETLIntMap* map);

void* ketlIntMapGet(KETLIntMap* map, KETLIntMapKey key);

bool ketlIntMapGetOrCreate(KETLIntMap* map, KETLIntMapKey key, void* ppValue);

void ketlIntMapReset(KETLIntMap* map);

KETL_DEFINE(KETLIntMapIterator) {
	KETLIntMap* map;
	uint64_t currentIndex;
	KETLIntMapBucketBase* currentBucket;
};

void ketlInitIntMapIterator(KETLIntMapIterator* iterator, KETLIntMap* map);

bool ketlIntMapIteratorHasNext(KETLIntMapIterator* iterator);

void ketlIntMapIteratorGet(KETLIntMapIterator* iterator, KETLIntMapKey* pKey, void* ppValue);

void ketlIntMapIteratorNext(KETLIntMapIterator* iterator);

void ketlIntMapIteratorRemove(KETLIntMapIterator* iterator);

#endif /*int_map_h*/
