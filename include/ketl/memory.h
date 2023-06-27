//🍲ketl
#ifndef memory_h
#define memory_h

#include "utils.h"

#include <inttypes.h>

typedef void* (*KETLAllocatorAlloc)(size_t size, void* userInfo);
typedef void* (*KETLAllocatorRealloc)(void* ptr, size_t size, void* userInfo);
typedef void (*KETLAllocatorFree)(void* ptr, void* userInfo);

KETL_FORWARD(KETLAllocator);

struct KETLAllocator {
	KETLAllocatorAlloc alloc;
	KETLAllocatorRealloc realloc;
	KETLAllocatorFree free;
	void* userInfo;
};

#endif /*memory_h*/
