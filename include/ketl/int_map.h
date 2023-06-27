//🍲ketl
#ifndef int_map_h
#define int_map_h

#include "ketl/object_pool.h"
#include "ketl/utils.h"

#include <inttypes.h>

typedef uint64_t KETLIntMapKey;

KETL_FORWARD(KETLIntMap);

KETL_FORWARD(KETLIntMapBucketBase);

struct KETLIntMap {
	KETLObjectPool bucketPool;
	KETLIntMapBucketBase** buckets;
	uint64_t size;
	uint64_t capacityIndex;
};

void ketlInitIntMap(KETLIntMap* map, size_t objectSize, size_t poolSize);

void ketlDeinitIntMap(KETLIntMap* map);

void* ketlIntMapPut(KETLIntMap* map, KETLIntMapKey key);

#endif /*int_map_h*/
