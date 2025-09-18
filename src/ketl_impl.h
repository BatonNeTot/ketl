#include "ketl/ketl.h"

#include "compiler/hir.h"

#include "type_impl.h"
#include "gc_memory.h"

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
    ketl_namespace global_namespace;
    
    function_types_map m_function_types;

    operator_overloading_map am_hiroperator_overloading[
        ((KETL_HIR_LAST_BI_OPERATOR >> KETL_HIR_TYPE_INSTR_SHIFT) + 1) - (KETL_HIR_FIRST_BI_OPERATOR >> KETL_HIR_TYPE_INSTR_SHIFT)
    ];
};
