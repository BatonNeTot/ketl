#include "ketl/ketl.h"

#include "bytecode.h"
#include "compiler/ir.h"

#include "type_impl.h"
#include "gc_memory.h"

#include "namespace.h"
#include "atomic_strings.h"

#include "containers/vector.h"
#include "containers/hash_map.h"

KETL_DEFINE(function_parameters) {
    ketl_type_parameter* pParameters;
    uint16_t parametersCount;
};

KETL_HASH_MAP_DECLARATION(function_types_map, function_parameters, ketl_type_function*)

KETL_VECTOR_DECLARATION(types, ketl_type*)

KETL_HASH_MAP_DECLARATION(operator_overloading_map, function_parameters, ketl_bytecode_instr)

KETL_DEFINE(ketl_state) {
    const ketl_allocator* pAllocator;
    ketl_gc gc;
    ketl_atomic_strings atomicStrings;
    ketl_namespace globalNamespace;
    
    function_types_map mFunctionTypes;

    operator_overloading_map amOperatorOverloading[KETL_IR_LAST_OPERATOR + 1 - KETL_IR_FIRST_OPERATOR];
};

ketl_type* ketl_state_find_type(const char* pName);
