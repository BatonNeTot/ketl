//🫖ketl
#ifndef ketl_executable_memory_h
#define ketl_executable_memory_h

#include "containers/vector.h"

#include "ketl/memory.h"
#include "ketl/utils.h"

ANN_DEFINE(ketl_executable_memory_page) {
	uint8_t* pPage;
	uint32_t pageSize;
};

KETL_VECTOR_DECLARATION(ketl_executable_memory_page_vector, ketl_executable_memory_page)

ANN_DEFINE(ketl_executable_memory) {
	ketl_executable_memory_page_vector vPages;
	uint32_t currentOffset;
};

void ketl_executable_memory_init(ketl_executable_memory* exeMemory, const ketl_allocator* p_allocator);

void ketl_executable_memory_deinit(ketl_executable_memory* exeMemory);

uint8_t* ketl_executable_memory_allocate(ketl_executable_memory* exeMemory, const uint8_t* opcodes, uint64_t length);

#endif // ketl_executable_memory_h
