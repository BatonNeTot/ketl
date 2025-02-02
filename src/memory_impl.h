//🫖ketl
#ifndef ketl_memory_impl_h
#define ketl_memory_impl_h

#include "ketl/memory.h"


void* ketl_alloc(const ketl_allocator* allocator, size_t size);

void* ketl_realloc(const ketl_allocator* allocator, void* ptr, size_t size);

void ketl_free(const ketl_allocator* allocator, void* ptr);

void ketl_memset(void* dest, unsigned char val, size_t size);

void ketl_memcpy(void* dest, const void* src, size_t size);

#endif // ketl_memory_impl_h
