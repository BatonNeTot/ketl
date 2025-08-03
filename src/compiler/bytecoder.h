//🫖ketl
#ifndef ketl_compiler_bytecoder_h
#define ketl_compiler_bytecoder_h

#include "namespace.h"
#include "atomic_strings.h"
#include "bytecode.h"
#include "hir.h"

#include "ketl/memory.h"
#include "ketl/utils.h"

KETL_FORWARD(ketl_state);

ketl_bytecode ketl_bytecode_compile_from_hir(ketl_state* pState, ketl_hir_t* p_hir, const ketl_allocator* p_allocator);  

#endif // ketl_compiler_bytecoder_h
