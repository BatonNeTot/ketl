#include "ketl/ketl.h"

#include "bytecode.h"
#include "compiler/hir.h"

#include "type_impl.h"
#include "gc_memory.h"

#include "namespace.h"
#include "atomic_strings.h"

#include "containers/vector.h"
#include "containers/hash_map.h"

KETL_DEFINE(function_type_composite) {
    ketl_type_signature* pSignature;
    ketl_type_function* pFuncType;
    ketl_type_function* pCFuncType;
};

KETL_HASH_MAP_DECLARATION(function_types_map, ketl_function_parameters, function_type_composite)

KETL_VECTOR_DECLARATION(types, ketl_type*)
KETL_VECTOR_DECLARATION(string_builder_t, char)

KETL_HASH_MAP_DECLARATION(operator_overloading_map, ketl_function_parameters, ketl_bytecode_instr)

KETL_DEFINE(ketl_state) {
    const ketl_allocator* pAllocator;
    ketl_gc gc;
    string_builder_t error_stream;
    ketl_atomic_strings atomicStrings;
    ketl_namespace globalNamespace;
    
    function_types_map mFunctionTypes;

    operator_overloading_map amHIROperatorOverloading[KETL_HIR_LAST_UNDEF_OPERATOR + 1 - KETL_HIR_FIRST_UNDEF_OPERATOR];
};
