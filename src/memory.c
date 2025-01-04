//🫖ketl
#include "memory_impl.h"

#include <stdlib.h>

static void* ketl_default_alloc(size_t size, void* userInfo) {
	(void)userInfo;
	return malloc(size);
}

static void* ketl_default_realloc(void* ptr, size_t size, void* userInfo) {
	(void)userInfo;
	return realloc(ptr, size);
}

static void ketl_default_free(void* ptr, void* userInfo) {
	(void)userInfo;
	free(ptr);
}

ketl_allocator ketl_default_allocator = {
	&ketl_default_alloc,
	&ketl_default_realloc,
	&ketl_default_free,
	NULL
};


void* ketl_alloc(ketl_allocator* allocator, size_t size) {
	return allocator->alloc(size, allocator->userInfo);
}

void* ketl_realloc(ketl_allocator* allocator, void* ptr, size_t size) {
	return allocator->realloc(ptr, size, allocator->userInfo);
}

void ketl_free(ketl_allocator* allocator, void* ptr) {
	allocator->free(ptr, allocator->userInfo);
}

void ketl_memset(void* dest, unsigned char val, size_t size) {
	memset(dest, val, size);
}

void ketl_memcpy(void* dest, const void* src, size_t size) {
	memcpy(dest, src, size);
}
