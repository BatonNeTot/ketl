#include "ketl/ketl.h"

#include "compiler/lexer.h"
#include "compiler/hir.h"

#include "type_impl.h"
#include "gc_memory.h"
#include "executable_memory.h"

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

KETL_VECTOR_DECLARATION(types, ketl_type*)
KETL_VECTOR_DECLARATION(string_builder_t, char)

KETL_HASH_MAP_DECLARATION(operator_overloading_map, ketl_function_parameters, ketl_hir_tag_t)

ANN_DEFINE(ketl_state) {
    const ketl_allocator* p_allocator;
    ketl_gc gc;
    string_builder_t error_stream;
    ketl_atomic_strings atomic_strings;
    ketl_executable_memory executable_memory;
    ketl_namespace global_namespace;
    
    function_types_map m_function_types;

    operator_overloading_map am_hiroperator_overloading[
        ((KETL_HIR_LAST_BI_OPERATOR >> KETL_HIR_TYPE_INSTR_SHIFT) + 1) - (KETL_HIR_FIRST_BI_OPERATOR >> KETL_HIR_TYPE_INSTR_SHIFT)
    ];
};

void ketl_state_define_function_impl(ketl_state* p_state, const char* p_name, uint32_t length, ketl_type* p_type, void* p_func, bool force);

void* ketl_state_compile_function(ketl_state* p_state, ketl_lexer_t* p_lexer, uint32_t* p_opcodes_size, ketl_named_variable_type_info_t* p_parameters, uint32_t parameter_count, ketl_variable* p_output_variable);
