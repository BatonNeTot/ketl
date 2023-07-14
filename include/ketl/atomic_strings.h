//🍲ketl
#ifndef atomic_strings_h
#define atomic_strings_h

#include "ketl/int_map.h"
#include "ketl/utils.h"

KETL_FORWARD(KETLAtomicStringsBucketBase);

KETL_DEFINE(KETLAtomicStrings) {
	KETLObjectPool bucketPool;
	KETLObjectPool stringPool;
	KETLAtomicStringsBucketBase** buckets;
	uint64_t size;
	uint64_t capacityIndex;
};

uint64_t ketlHashString(const char* str, uint64_t length);

void ketlInitAtomicStrings(KETLAtomicStrings* strings, size_t poolSize);

void ketlDeinitAtomicStrings(KETLAtomicStrings* strings);

const char* ketlAtomicStringsGet(KETLAtomicStrings* strings, const char* key, uint64_t length);

#endif /*atomic_strings_h*/
