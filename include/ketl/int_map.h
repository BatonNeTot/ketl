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

bool ketlIntMapGet(KETLIntMap* map, KETLIntMapKey key, void* value);

#endif /*int_map_h*/
