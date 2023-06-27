//🍲ketl
#ifndef memory_impl_h
#define memory_impl_h

#include "ketl/memory.h"

inline void* KETLAlloc(KETLAllocator* allocator, size_t size) {
	return allocator->alloc(size, allocator->userInfo);
}

inline void* KETLRealloc(KETLAllocator* allocator, void* ptr, size_t size) {
	return allocator->realloc(ptr, size, allocator->userInfo);
}

inline void KETLFree(KETLAllocator* allocator, void* ptr) {
	allocator->free(ptr, allocator->userInfo);
}

#endif /*memory_impl_h*/
