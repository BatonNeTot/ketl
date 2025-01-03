//🫖ketl
#ifndef ketl_executable_memory_h
#define ketl_executable_memory_h

#include "containers/vector.h"
#include "ketl/utils.h"

KETL_DEFINE(ketl_executable_memory_page) {
	uint8_t* pPage;
	uint32_t pageSize;
};

KETL_NAMED_VECTOR_DECLARATION(executable_memory_page, ketl_executable_memory_page)

KETL_DEFINE(ketl_executable_memory) {
	ketl_vector_executable_memory_page pages;
	uint32_t currentOffset;
};

void ketl_executable_memory_init(ketl_executable_memory* exeMemory);

void ketl_executable_memory_deinit(ketl_executable_memory* exeMemory);

uint8_t* ketl_executable_memory_allocate(ketl_executable_memory* exeMemory, const uint8_t* opcodes, uint64_t length);

#endif // ketl_executable_memory_h