//🫖ketl
#ifndef ketl_memory_h
#define ketl_memory_h

#include "ketl/utils.h"

typedef void* (*ketl_allocator_alloc_f)(size_t size, void* userInfo);
typedef void* (*ketl_allocator_realloc_f)(void* ptr, size_t size, void* userInfo);
typedef void (*ketl_allocator_free_f)(void* ptr, void* userInfo);

KETL_DEFINE(ketl_allocator) {
	ketl_allocator_alloc_f alloc;
	ketl_allocator_realloc_f realloc;
	ketl_allocator_free_f free;
	void* userInfo;
};

extern const ketl_allocator ketl_default_allocator;

#endif // ketl_memory_h
