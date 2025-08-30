//🫖ketl
#include "ketl_impl.h"

#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "compiler/bytecoder.h"
#include "compiler/assembler.h"

#include "executable_memory.h"
#include "value_impl.h"
#include "type_impl.h"
#include "memory_impl.h"

#include <stdio.h>

#define FUNC_SIGNATURE_HASH(key) func_signature_hash(&(key))

static uint64_t func_signature_hash(const ketl_function_parameters* pParameters) {
    uint64_t hash = 0u;
    uint16_t parametersCount = pParameters->parametersCount;
    for (uint16_t i = 0u; i < parametersCount; ++i) {
        hash = ((uint64_t)pParameters->pParameters[i].p_type) ^ (hash << 1);
    }
    return hash;
}

#define IS_FUNC_SIGNATURES_EQUAL(lhsKey, rhsKey) is_func_signatures_equal(&(lhsKey), &(rhsKey))

static bool is_func_signatures_equal(const ketl_function_parameters* pLhsParameters, const ketl_function_parameters* pRhsParameters) {
    if (pLhsParameters->parametersCount != pRhsParameters->parametersCount) {
        return false;
    }
    uint16_t parametersCount = pLhsParameters->parametersCount;
    for (uint16_t i = 0u; i < parametersCount; ++i) {
        if (pLhsParameters->pParameters[i].p_type != pRhsParameters->pParameters[i].p_type) {
            return false;
        }
    }
    return true;
}

#define FUNC_PARAMETERS_HASH(key) func_parameters_hash(&(key))

static uint64_t func_parameters_hash(const ketl_function_parameters* pParameters) {
    uint64_t hash = 0u;
    uint16_t parametersCount = pParameters->parametersCount;
    // first is return type, ignore it for parameters
    for (uint16_t i = 1u; i < parametersCount; ++i) {
        hash = ((uint64_t)pParameters->pParameters[i].p_type) ^ (hash << 1);
    }
    return hash;
}

#define IS_FUNC_PARAMETERS_EQUAL(lhsKey, rhsKey) is_func_parameters_equal(&(lhsKey), &(rhsKey))

static bool is_func_parameters_equal(const ketl_function_parameters* pLhsParameters, const ketl_function_parameters* pRhsParameters) {
    if (pLhsParameters->parametersCount != pRhsParameters->parametersCount) {
        return false;
    }
    uint16_t parametersCount = pLhsParameters->parametersCount;
    // first is return type, ignore it for parameters
    for (uint16_t i = 1u; i < parametersCount; ++i) {
        if (pLhsParameters->pParameters[i].p_type != pRhsParameters->pParameters[i].p_type) {
            return false;
        }
    }
    return true;
}

KETL_HASH_MAP_DEFINITION(function_types_map, ketl_function_parameters, function_type_composite, FUNC_SIGNATURE_HASH, IS_FUNC_SIGNATURES_EQUAL)

KETL_VECTOR_DEFINITION(types, ketl_type*)
KETL_VECTOR_DEFINITION(string_builder_t, char)

KETL_HASH_MAP_DEFINITION(operator_overloading_map, ketl_function_parameters, ketl_bytecode_instr, FUNC_PARAMETERS_HASH, IS_FUNC_PARAMETERS_EQUAL)

static const function_type_composite* get_function_type_composite(ketl_state* pState, const ketl_function_parameters* pParameters) {
    uint16_t parametersCount = pParameters->parametersCount;
    function_types_map_bucket* pBucket = function_types_map_get_or_insert_copy(&pState->mFunctionTypes, *pParameters, (function_type_composite){NULL, NULL, NULL});
    if (pBucket->value.pSignature == NULL) {
        uint64_t signatureSize = sizeof(ketl_type_signature) + parametersCount * sizeof(ketl_type_parameter);
        uint64_t functionsOffset = ANN_ALIGN_FORWARD(signatureSize, _Alignof(ketl_type_function));
        uint64_t totalAllocSize = functionsOffset + 2 * sizeof(ketl_type_function);
        void* pAllocMem = ketl_alloc(pState->p_allocator, totalAllocSize);

        ketl_type_signature* pSignature = pAllocMem;
        *pSignature = (ketl_type_signature){
            .parametersCount = parametersCount
        };
        ketl_memcpy(pSignature->aParameters, pParameters->pParameters, parametersCount * sizeof(ketl_type_parameter));

        pBucket->key.pParameters = pSignature->aParameters;
        pBucket->value.pSignature = pSignature;

        ketl_type_function* pFuncTypes = (ketl_type_function*)((char*)pAllocMem + functionsOffset);
        *pFuncTypes = (ketl_type_function){
            .type = KETL_TYPE_FUNCTION,
            .align = _Alignof(void(*)(void)),
            .size = sizeof(void(*)(void)),
            .pTypeSignature = pSignature
        };
        *(pFuncTypes + 1) = (ketl_type_function){
            .type = KETL_TYPE_CFUNCTION,
            .align = _Alignof(void(*)(void)),
            .size = sizeof(void(*)(void)),
            .pTypeSignature = pSignature
        };
        pBucket->value.pFuncType = pFuncTypes;
        pBucket->value.pCFuncType = pFuncTypes + 1;
    }
    return &pBucket->value;
}

ketl_state* ketl_state_create(const ketl_allocator* p_allocator) {
    ketl_state* pState = ketl_alloc(p_allocator, sizeof(ketl_state));
    *pState = (ketl_state){
        .p_allocator = p_allocator
    };

    ketl_gc_init(&pState->gc, p_allocator);
    string_builder_t_init(&pState->error_stream, 16, pState->p_allocator);
    ketl_atomic_strings_init(&pState->atomicStrings, p_allocator);
    ketl_namespace_init(&pState->globalNamespace, p_allocator);
    function_types_map_init(&pState->mFunctionTypes, p_allocator);

    for (uint32_t i = 0; i < ANN_ARRAY_SIZE(pState->amHIROperatorOverloading); ++i) {
        operator_overloading_map_init(pState->amHIROperatorOverloading + i, p_allocator);
    }

#define INIT_TYPE(_var, _type) *(_type*)(_var) = (_type)
#define CREATE_PRIMITIVE_TYPE(_varName, _name, _size, _isInteger, _isSigned)\
ketl_type* _varName = ketl_alloc(p_allocator, sizeof(ketl_type_primitive)); \
do {\
ketl_atomic_string sName = ketl_atomic_strings_get(&pState->atomicStrings, _name, sizeof(_name) - 1);\
INIT_TYPE(_varName, ketl_type_primitive) {\
        .sName = sName,\
        .type = KETL_TYPE_PRIMITIVE,\
        .align = _size,\
        .size = _size,\
        .isInteger = _isInteger,\
        .isSigned = _isSigned,\
        };\
ketl_variable namespaceVariable = {\
    .type = KETL_VARIABLE_TYPE,\
    .p_type = NULL,\
    .pointer = _varName,\
};\
ketl_namespace_put(&pState->globalNamespace, sName, namespaceVariable);\
} while(0)

    CREATE_PRIMITIVE_TYPE(tNone,  "none", 0, false, false);
    CREATE_PRIMITIVE_TYPE(tBool,  "bool", 1, false, false);
    CREATE_PRIMITIVE_TYPE(tInt8,  "i8",   1, true,  true);
    CREATE_PRIMITIVE_TYPE(tInt16, "i16",  2, true,  true);
    CREATE_PRIMITIVE_TYPE(tInt32, "i32",  4, true,  true);
    CREATE_PRIMITIVE_TYPE(tInt64, "i64",  8, true,  true);

#define REGISTER_BINARY_OPERATOR(_hir_tag_op, _argType, _returnType, _hir_tag_typed_op)\
do {\
    ketl_type_parameter parametersArray[] = { {.p_type = _returnType}, {.p_type = _argType}, {.p_type = _argType} };\
    ketl_function_parameters parameters = {\
        .pParameters = parametersArray,\
        .parametersCount = sizeof(parametersArray) / sizeof(*parametersArray)\
    };\
\
    const function_type_composite* pFuncTypeComposite = get_function_type_composite(pState, &parameters);\
    parameters.pParameters = pFuncTypeComposite->pSignature->aParameters;\
\
    operator_overloading_map_get_or_insert_copy(pState->amHIROperatorOverloading + (_hir_tag_op - KETL_HIR_FIRST_UNDEF_OPERATOR), parameters, _hir_tag_typed_op);\
} while (false)

#define REGISTER_PRIMITIVE_BINARY_OPERATORS(_argType, _hir_first_typed_op)\
do {\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 0,  _argType, _argType, _hir_first_typed_op + 0);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 1,  _argType, _argType, _hir_first_typed_op + 1);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 2,  _argType, _argType, _hir_first_typed_op + 2);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 3,  _argType, _argType, _hir_first_typed_op + 3);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 4,  _argType, _argType, _hir_first_typed_op + 4);\
\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 5,  _argType, tBool, _hir_first_typed_op + 5);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 6,  _argType, tBool, _hir_first_typed_op + 6);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 7,  _argType, tBool, _hir_first_typed_op + 7);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 8,  _argType, tBool, _hir_first_typed_op + 8);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 9,  _argType, tBool, _hir_first_typed_op + 9);\
    REGISTER_BINARY_OPERATOR(KETL_HIR_FIRST_UNDEF_OPERATOR + 10, _argType, tBool, _hir_first_typed_op + 10);\
} while (false)

    REGISTER_PRIMITIVE_BINARY_OPERATORS(tInt8,  KETL_HIR_FIRST_I8_OPERATOR);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(tInt16, KETL_HIR_FIRST_I16_OPERATOR);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(tInt32, KETL_HIR_FIRST_I32_OPERATOR);
    REGISTER_PRIMITIVE_BINARY_OPERATORS(tInt64, KETL_HIR_FIRST_I64_OPERATOR);

    return pState;
}

void ketl_state_destroy(ketl_state* pState) {
    for (uint32_t i = 0; i < ANN_ARRAY_SIZE(pState->amHIROperatorOverloading); ++i) {
        operator_overloading_map_deinit(pState->amHIROperatorOverloading + i);
    }

    KETL_HASH_MAP_FOREACH(function_types_map, &pState->mFunctionTypes,
        ketl_free(pState->p_allocator, __pBucket->value.pSignature);    
    );
    function_types_map_deinit(&pState->mFunctionTypes);

#define FREE_PRIMITIVE_TYPE(_name)\
do {\
ketl_atomic_string sTypeName = ketl_atomic_strings_get(&pState->atomicStrings, _name, sizeof(_name) - 1);\
ketl_namespace_node* pTypeNode = ketl_namespace_find(&pState->globalNamespace, sTypeName);\
ANN_ASSERT(pTypeNode->variable.type == KETL_VARIABLE_TYPE);\
ketl_free(pState->p_allocator, pTypeNode->variable.pointer);\
} while(0)

    FREE_PRIMITIVE_TYPE("none");
    FREE_PRIMITIVE_TYPE("bool");
    FREE_PRIMITIVE_TYPE("i64");

    ketl_namespace_deinit(&pState->globalNamespace);
    ketl_atomic_strings_deinit(&pState->atomicStrings);
    string_builder_t_deinit(&pState->error_stream);
    ketl_gc_deinit(&pState->gc);

    ketl_free(pState->p_allocator, pState);
}

// TODO FIX might be called often, replace allocation on heap with field in ketl_state
ketl_type* ketl_state_get_none_type(ketl_state* pState) {
    return ketl_state_get_type(pState, "none", 4);
}

ketl_type* ketl_state_get_i8(ketl_state* pState) {
    return ketl_state_get_type(pState, "i8", 2);
}

ketl_type* ketl_state_get_i16(ketl_state* pState) {
    return ketl_state_get_type(pState, "i16", 3);
}

ketl_type* ketl_state_get_i32(ketl_state* pState) {
    return ketl_state_get_type(pState, "i32", 3);
}

ketl_type* ketl_state_get_i64(ketl_state* pState) {
    return ketl_state_get_type(pState, "i64", 3);
}

ketl_type* ketl_state_get_type(ketl_state* p_state, const char* p_type_name, uint32_t length) {
    ketl_atomic_string a_type_name = ketl_atomic_strings_get(&p_state->atomicStrings, p_type_name, length);
    ketl_namespace_node* p_type_node = ketl_namespace_find(&p_state->globalNamespace, a_type_name);
    ANN_ASSERT(p_type_node->variable.type == KETL_VARIABLE_TYPE);
    return p_type_node->variable.pointer;
}

ketl_type* ketl_state_get_function_type(ketl_state* pState, const ketl_function_parameters* pParameters) {
    return (ketl_type*)get_function_type_composite(pState, pParameters)->pFuncType;
}

ketl_type* ketl_state_get_cfunction_type(ketl_state* pState, const ketl_function_parameters* pParameters) {
    return (ketl_type*)get_function_type_composite(pState, pParameters)->pCFuncType;
}

void ketl_state_define_function(ketl_state* pState, const char* pName, uint32_t length, ketl_type* p_type, void* pFunc) {
    ketl_atomic_string sName = ketl_atomic_strings_get(&pState->atomicStrings, pName, length);
    ketl_variable namespaceVariable = {
        .type = KETL_VARIABLE_POINTER,
        .p_type = p_type,
        .pointer = pFunc,
    };
    ketl_namespace_put(&pState->globalNamespace, sName, namespaceVariable);
}

ketl_value* ketl_state_eval(ketl_state* pState, const char* p_filename, const char* p_source, uint32_t length) {
    ketl_variable output_variable;

    ////////////////////////////////

    ketl_lexer_t lexer;
    ketl_lexer_init(&lexer, pState->p_allocator);
    ketl_lexer_build_tokens(&lexer, p_source, length);

    ////////////////////////////////

    ketl_hir_t hir;
    ketl_parser_build_hir(pState, &hir, p_filename, &lexer, pState->p_allocator);
    ketl_lexer_deinit(&lexer);
    if (pState->error_stream.size > 0) {
        // TODO return error
        printf("%.*s", pState->error_stream.size, pState->error_stream.p_data);
        pState->error_stream.size = 0;

        ketl_variable_set_type(&output_variable, ketl_state_get_none_type(pState));
        return ketl_value_from_variable(output_variable, pState->p_allocator);
    }
    ketl_variable_set_type(&output_variable, hir.p_used_types[hir.return_type]);

    {
        char arr_buffer[1024];
        uint32_t length = ketl_hir_format(&hir, arr_buffer, ANN_ARRAY_SIZE(arr_buffer));
        printf("%.*s", length, arr_buffer);
    }
    
#if ANN_IS_DEBUG
    for (uint32_t i = 0u; i < hir.vars_count; ++i) {
        if (hir.p_vars[i].type == KETL_HIR_USED_TYPE_UNKNOWN) {
            // TODO error debug only
            // cause in release we want it to finish building and show all of the errors
            ANN_ASSERT(false);
        } 
    }
#endif

    printf("-------------------------------\n");

    ketl_bytecode bytecode = ketl_bytecode_compile_from_hir(pState, &hir, pState->p_allocator);
    ketl_hir_deinit(&hir);

    for (uint32_t i = 0u; i < bytecode.instructionsCount; 
            i += ketl_bytecode_decode_instruction_length(bytecode.pInstructions[i])) {
        char arr_buffer[256];
        uint32_t length = ketl_bytecode_format(bytecode.pInstructions + i, bytecode.pLabels, arr_buffer, ANN_ARRAY_SIZE(arr_buffer));
        printf("%d: %.*s\n", i, length, arr_buffer);
    }

    printf("-------------------------------\n");

    uint32_t opcodesSize = 0u;
    uint8_t* pOpcodes = ketl_assembler_compile(bytecode, &opcodesSize, pState->p_allocator);
    ketl_free(pState->p_allocator, bytecode.pInstructions);

    {
        char arr_buffer[2048];
        uint32_t length = ketl_assembler_format(pOpcodes, opcodesSize, arr_buffer, ANN_ARRAY_SIZE(arr_buffer));
        printf("%.*s\n", length, arr_buffer);
    }

    ketl_executable_memory ex_memory;
    ketl_executable_memory_init(&ex_memory, pState->p_allocator);

    uint8_t* executableOpcodes = ketl_executable_memory_allocate(&ex_memory, pOpcodes, opcodesSize);
    ketl_free(pState->p_allocator, pOpcodes);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    uint64_t(*func)(void) = (uint64_t(*)(void))executableOpcodes;
#pragma GCC diagnostic pop

    output_variable.uint64 = func();

    ketl_executable_memory_deinit(&ex_memory);

    pState->error_stream.size = 0;

    return ketl_value_from_variable(output_variable, pState->p_allocator);
}
