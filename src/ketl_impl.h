#include "ketl/ketl.h"

#include "compiler/lexer.h"
#include "compiler/hir.h"

#include "type_impl.h"
#include "gc_memory.h"
#include "executable_memory.h"
#include "module.h"

#include "namespace.h"
#include "atomic_strings.h"

#include "containers/vector.h"
#include "containers/hash_map.h"

ANN_DEFINE(function_type_composite) {
    ketl_type_signature* p_signature;
    ketl_type_function* p_func_type;
    ketl_type_function* p_cfunc_type;
};

KETL_HASH_MAP_DECLARATION(function_types_map, ketl_function_parameters, function_type_composite)
KETL_HASH_MAP_DECLARATION(array_types_map_t, ketl_type*, ketl_type*)

KETL_VECTOR_DECLARATION(types, ketl_type*)
KETL_VECTOR_DECLARATION(string_builder_t, char)

KETL_HASH_MAP_DECLARATION(ketl_modules_t, ketl_atomic_string, ketl_module_t)

KETL_HASH_MAP_DECLARATION(operator_overloading_map, ketl_function_parameters, ketl_hir_tag_t)

ANN_DEFINE(ketl_state) {
    const ketl_allocator* p_allocator;
    ketl_gc gc;
    string_builder_t error_stream;
    ketl_atomic_strings atomic_strings;
    ketl_executable_memory executable_memory;
    ketl_namespace global_namespace;
    ketl_modules_t modules;
    
    function_types_map function_types;
    array_types_map_t array_types;

    operator_overloading_map am_hiroperator_overloading[
        ((KETL_HIR_LAST_BI_OPERATOR >> KETL_HIR_TYPE_INSTR_SHIFT) + 1) - (KETL_HIR_FIRST_BI_OPERATOR >> KETL_HIR_TYPE_INSTR_SHIFT)
    ];

    compile_function_declarations_t compile_function_declarations;
    bool loading_modules;
};

void ketl_state_postload(ketl_state* p_state, ketl_lexer_t* p_lexer, ketl_namespace* p_namespace, compile_function_declarations_t* p_compile_function_declarations);

bool ketl_state_load_module_impl(ketl_state* p_state, ketl_atomic_string s_module_name, ketl_namespace* p_namespace);

ketl_type* ketl_state_get_type_impl(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_type_name, uint32_t length);

ketl_namespace_node* ketl_state_define_var(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_type* p_type);

ketl_namespace_node* ketl_state_define_function(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_type* p_type, void(*cfunc)(void));

ketl_namespace_node* ketl_state_define_class(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_named_variable_type_info_t* p_fields, uint16_t field_count);

ketl_namespace_node* ketl_state_define_enum(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_type_primitive* p_parent_primitive, ketl_type_enum_pair* p_constants, uint64_t constant_count);

ketl_namespace_node* ketl_state_define_mimic(ketl_state* p_state, ketl_namespace* p_namespace, const char* p_name, uint32_t length, ketl_type* p_type);

void* ketl_state_compile_function(ketl_state* p_state, ketl_lexer_t* p_lexer, ketl_token_iterator_t end_pos, ketl_namespace* p_namespace, uint32_t* p_opcodes_size, ketl_named_variable_type_info_t* p_parameters, uint32_t parameter_count, bool vars_in_namespace, ketl_variable* p_output_variable);
