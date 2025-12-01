//🫖ketl
#include "memory_impl.h"

#include <stdlib.h>
#include <string.h>

static void* ketl_default_alloc(size_t size, void* user_info) {
	(void)user_info;
	return malloc(size);
}

static void* ketl_default_realloc(void* ptr, size_t size, void* user_info) {
	(void)user_info;
	return realloc(ptr, size);
}

static void ketl_default_free(void* ptr, void* user_info) {
	(void)user_info;
	free(ptr);
}

const ketl_allocator ketl_default_allocator = {
	&ketl_default_alloc,
	&ketl_default_realloc,
	&ketl_default_free,
	NULL
};


void* ketl_alloc(const ketl_allocator* allocator, size_t size) {
	return allocator->alloc(size, allocator->user_info);
}

void* ketl_realloc(const ketl_allocator* allocator, void* ptr, size_t size) {
	return allocator->realloc(ptr, size, allocator->user_info);
}

void ketl_free(const ketl_allocator* allocator, void* ptr) {
	allocator->free(ptr, allocator->user_info);
}

void ketl_memset(void* dest, unsigned char val, size_t size) {
	memset(dest, val, size);
}

void ketl_memcpy(void* dest, const void* src, size_t size) {
	memcpy(dest, src, size);
}

uint64_t ketl_strlen(const char* p_str) {
	if (p_str == NULL) {
		return 0;
	}
	return strlen(p_str);
}
