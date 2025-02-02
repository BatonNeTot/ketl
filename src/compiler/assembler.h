//🫖ketl
#ifndef ketl_compiler_assembler_h
#define ketl_compiler_assembler_h

#include "bytecode.h"

#include "ketl/memory.h"
#include "ketl/utils.h"


uint8_t* ketl_assembler_compile(ketl_bytecode bytecode, uint32_t* pOpcodesSize, const ketl_allocator* pAllocator);

uint32_t ketl_assembler_format(uint8_t* pOpcodes, uint32_t opcodesSize, char* buffer, uint32_t bufferSize);

#endif // ketl_compiler_assembler_h