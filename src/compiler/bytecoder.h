//🫖ketl
#ifndef ketl_compiler_bytecoder_h
#define ketl_compiler_bytecoder_h

#include "namespace.h"
#include "atomic_strings.h"
#include "bytecode.h"
#include "ir.h"

#include "ketl/memory.h"
#include "ketl/utils.h"

KETL_FORWARD(ketl_state);

ketl_bytecode ketl_bytecode_compile(ketl_state* pState, ketl_ir ir, const ketl_allocator* pAllocator); 

#endif // ketl_compiler_bytecoder_h
