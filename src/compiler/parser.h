//🫖ketl
#ifndef ketl_compiler_parser_h
#define ketl_compiler_parser_h

#include "compiler/hir_builder.h"

#include "ketl/memory.h"
#include "ketl/utils.h"

ANN_FORWARD(ketl_state);

void ketl_parser_build_hir(ketl_state* p_state, ketl_hir_t* p_hir, const char* p_filename, const char* p_source, uint32_t length, const ketl_allocator* p_allocator);

void ketl_simple_parser_build_hir(ketl_state* p_state, ketl_hir_t* p_hir, const char* p_filename, const char* p_source, uint32_t length, const ketl_allocator* p_allocator);

#endif // ketl_compiler_parser_h
