//🫖ketl
#ifndef ketl_compiler_bytecoder_h
#define ketl_compiler_bytecoder_h

#include "bytecode.h"
#include "ir.h"

#include "ketl/memory.h"
#include "ketl/utils.h"

ketl_bytecode ketl_bytecode_compile(ketl_ir ir, const ketl_allocator* pAllocator); 

#endif // ketl_compiler_bytecoder_h