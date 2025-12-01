//🫖ketl
#ifndef ketl_compiler_parser_h
#define ketl_compiler_parser_h

#include "compiler/hir_builder.h"

#include "ketl/memory.h"
#include "ketl/utils.h"

ANN_FORWARD(ketl_state);
ANN_FORWARD(ketl_lexer_t);
ANN_FORWARD(ketl_namespace);

void ketl_parser_build_hir(ketl_state* p_state, ketl_hir_t* p_hir, ketl_lexer_t* p_lexer, ketl_namespace* p_namespace, ketl_named_variable_type_info_t* p_parameters, uint32_t parameter_count, const ketl_allocator* p_allocator);

#endif // ketl_compiler_parser_h
