//🫖ketl
#ifndef ketl_module_h
#define ketl_module_h

#include "namespace.h"
#include "atomic_strings.h"

#include "ketl/utils.h"

#include "type_impl.h"
#include "compiler/lexer.h"
#include "compiler/token.h"

ANN_FORWARD(ketl_state);


KETL_VECTOR_DECLARATION(ketl_parameters_t, ketl_named_variable_type_info_t)

ANN_DEFINE(compile_function_declaration_t) {
    ketl_atomic_string s_name;
    ketl_variable* p_variable;
    uint8_t* p_opcodes;
    uint64_t opcodes_size;
    ketl_token_iterator_t start_pos;
    ketl_token_iterator_t end_pos;
    ketl_parameters_t v_parameters;
};

KETL_VECTOR_DECLARATION(compile_function_declarations_t, compile_function_declaration_t)

ANN_DEFINE(ketl_module_t) {
    ketl_atomic_string s_name;
    ketl_namespace namespace;
    ketl_lexer_t lexer;
    compile_function_declarations_t compile_function_declarations;
    char *p_source;
    uint32_t opcodes_size;
    uint8_t* p_opcodes;
    bool header_loaded;
    bool body_loaded;
};

void ketl_module_init(ketl_module_t* p_module, ketl_atomic_string s_name, ketl_state* p_state);

void ketl_module_deinit(ketl_module_t* p_module);

bool ketl_module_preload(ketl_module_t* p_module, ketl_namespace* p_namespace, ketl_state* p_state);

bool ketl_module_load(ketl_module_t* p_module, ketl_state* p_state);

#endif
