//🫖ketl
#include "ketl_impl.h"

#include "compiler/parser.h"
#include "compiler/bytecoder.h"
#include "compiler/assembler.h"

#include "executable_memory.h"
#include "execution.h"
#include "type_impl.h"
#include "memory_impl.h"

#include <stdio.h>

#define FUNC_SIGNATURE_HASH(key) func_signature_hash(&(key))

static uint64_t func_signature_hash(const ketl_function_parameters* pParameters) {
    uint64_t hash = 0u;
    uint16_t parametersCount = pParameters->parametersCount;
    for (uint16_t i = 0u; i < parametersCount; ++i) {
        hash = ((uint64_t)pParameters->pParameters[i].pType) ^ (hash << 1);
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
        if (pLhsParameters->pParameters[i].pType != pRhsParameters->pParameters[i].pType) {
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
        hash = ((uint64_t)pParameters->pParameters[i].pType) ^ (hash << 1);
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
        if (pLhsParameters->pParameters[i].pType != pRhsParameters->pParameters[i].pType) {
            return false;
        }
    }
    return true;
}

KETL_HASH_MAP_DEFINITION(function_types_map, ketl_function_parameters, function_type_composite, FUNC_SIGNATURE_HASH, IS_FUNC_SIGNATURES_EQUAL)

KETL_VECTOR_DEFINITION(types, ketl_type*)

KETL_HASH_MAP_DEFINITION(operator_overloading_map, ketl_function_parameters, ketl_bytecode_instr, FUNC_PARAMETERS_HASH, IS_FUNC_PARAMETERS_EQUAL)

static const function_type_composite* get_function_type_composite(ketl_state* pState, const ketl_function_parameters* pParameters) {
    uint16_t parametersCount = pParameters->parametersCount;
    function_types_map_bucket* pBucket = function_types_map_get_or_insert_copy(&pState->mFunctionTypes, *pParameters, (function_type_composite){NULL, NULL, NULL});
    if (pBucket->value.pSignature == NULL) {
        uint64_t signatureSize = sizeof(ketl_type_signature) + parametersCount * sizeof(ketl_type_parameter);
        uint64_t functionsOffset = KETL_ALIGN_FORWARD(signatureSize, _Alignof(ketl_type_function));
        uint64_t totalAllocSize = functionsOffset + 2 * sizeof(ketl_type_function);
        void* pAllocMem = ketl_alloc(pState->pAllocator, totalAllocSize);

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

ketl_state* ketl_state_create(const ketl_allocator* pAllocator) {
    ketl_state* pState = ketl_alloc(pAllocator, sizeof(ketl_state));
    *pState = (ketl_state){
        .pAllocator = pAllocator
    };

    ketl_gc_init(&pState->gc, pAllocator);
    ketl_atomic_strings_init(&pState->atomicStrings, pAllocator);
    ketl_namespace_init(&pState->globalNamespace, pAllocator);
    function_types_map_init(&pState->mFunctionTypes, pAllocator);

    for (uint32_t i = 0; i < (sizeof(pState->amOperatorOverloading) / sizeof(*pState->amOperatorOverloading)); ++i) {
        operator_overloading_map_init(pState->amOperatorOverloading + i, pAllocator);
    }

#define INIT_TYPE(_var, _type) *(_type*)(_var) = (_type)
#define CREATE_PRIMITIVE_TYPE(_varName, _name, _size, _isInteger, _isSigned)\
ketl_type* _varName = ketl_alloc(pAllocator, sizeof(ketl_type_primitive)); \
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
    .pType = NULL,\
    .pointer = _varName,\
};\
ketl_namespace_put(&pState->globalNamespace, sName, namespaceVariable);\
} while(0)

    CREATE_PRIMITIVE_TYPE(tVoid, "void", 0, false, false);
    CREATE_PRIMITIVE_TYPE(tInt64, "i64", 8, true, true);

#define REGISTER_BINARY_OPERATOR(_operatorIr, _argType, _returnType, _bytecode)\
do {\
    ketl_type_parameter parametersArray[] = { {.pType = _returnType}, {.pType = _argType}, {.pType = _argType} };\
    ketl_function_parameters parameters = {\
        .pParameters = parametersArray,\
        .parametersCount = sizeof(parametersArray) / sizeof(*parametersArray)\
    };\
\
    const function_type_composite* pFuncTypeComposite = get_function_type_composite(pState, &parameters);\
    parameters.pParameters = pFuncTypeComposite->pSignature->aParameters;\
\
    operator_overloading_map_get_or_insert_copy(pState->amOperatorOverloading + (_operatorIr - KETL_IR_FIRST_OPERATOR), parameters, _bytecode);\
} while (false)

    REGISTER_BINARY_OPERATOR(KETL_IR_TYPE_PLUS, tInt64, tInt64, KETL_BYTECODE_64IADD);
    REGISTER_BINARY_OPERATOR(KETL_IR_TYPE_MULTIPLY, tInt64, tInt64, KETL_BYTECODE_64IMULTIPLY);

    return pState;
}

void ketl_state_destroy(ketl_state* pState) {
    for (uint32_t i = 0; i < (sizeof(pState->amOperatorOverloading) / sizeof(*pState->amOperatorOverloading)); ++i) {
        operator_overloading_map_deinit(pState->amOperatorOverloading + i);
    }

    KETL_HASH_MAP_FOREACH(function_types_map, function_parameters, ketl_type_function*, &pState->mFunctionTypes,
        ketl_free(pState->pAllocator, __pBucket->value.pSignature);    
    );
    function_types_map_deinit(&pState->mFunctionTypes);

#define FREE_PRIMITIVE_TYPE(_name)\
do {\
ketl_atomic_string sTypeName = ketl_atomic_strings_get(&pState->atomicStrings, _name, sizeof(_name) - 1);\
ketl_namespace_node* pTypeNode = ketl_namespace_find(&pState->globalNamespace, sTypeName);\
assert(pTypeNode->variable.type == KETL_VARIABLE_TYPE && pTypeNode->nextOffset == (uint32_t)(-1));\
ketl_free(pState->pAllocator, pTypeNode->variable.pointer);\
} while(0)

    FREE_PRIMITIVE_TYPE("void");
    FREE_PRIMITIVE_TYPE("i64");

    ketl_namespace_deinit(&pState->globalNamespace);
    ketl_atomic_strings_deinit(&pState->atomicStrings);
    ketl_gc_deinit(&pState->gc);

    ketl_free(pState->pAllocator, pState);
}

// TODO FIX might be called often, replace allocation on heap with field in ketl_state
ketl_type* ketl_state_get_void(ketl_state* pState) {
    ketl_atomic_string sVoidTypeName = ketl_atomic_strings_get(&pState->atomicStrings, "void", 4);
    ketl_namespace_node* pTypeNode = ketl_namespace_find(&pState->globalNamespace, sVoidTypeName);
    assert(pTypeNode->variable.type == KETL_VARIABLE_TYPE && pTypeNode->nextOffset == (uint32_t)(-1));
    return pTypeNode->variable.pointer;
}

ketl_type* ketl_state_get_i64(ketl_state* pState) {
    ketl_atomic_string sIntTypeName = ketl_atomic_strings_get(&pState->atomicStrings, "i64", 3);
    ketl_namespace_node* pTypeNode = ketl_namespace_find(&pState->globalNamespace, sIntTypeName);
    assert(pTypeNode->variable.type == KETL_VARIABLE_TYPE && pTypeNode->nextOffset == (uint32_t)(-1));
    return pTypeNode->variable.pointer;
}

ketl_type* ketl_state_get_function_type(ketl_state* pState, const ketl_function_parameters* pParameters) {
    return (ketl_type*)get_function_type_composite(pState, pParameters)->pFuncType;
}

ketl_type* ketl_state_get_cfunction_type(ketl_state* pState, const ketl_function_parameters* pParameters) {
    return (ketl_type*)get_function_type_composite(pState, pParameters)->pCFuncType;
}

void ketl_state_define_function(ketl_state* pState, const char* pName, uint32_t length, ketl_type* pType, void* pFunc) {
    ketl_atomic_string sName = ketl_atomic_strings_get(&pState->atomicStrings, pName, length);
    ketl_variable namespaceVariable = {
        .type = KETL_VARIABLE_POINTER,
        .pType = pType,
        .pointer = pFunc,
    };
    ketl_namespace_put(&pState->globalNamespace, sName, namespaceVariable);
}

void ketl_state_eval(ketl_state* pState, const char* pSource, uint32_t length) {
    ketl_state_eval_int64(pState, pSource, length);
}

int64_t ketl_state_eval_int64(ketl_state* pState, const char* pSource, uint32_t length) {
    ketl_ir ir = ketl_parser_parser(pSource, length, pState->pAllocator);

    for (uint32_t i = 0u; i < ir.nodesCount; ++i) {
        char aBuffer[256];
        uint32_t length = ketl_ir_node_format(ir.pNodes[i], ir.pSymbols, aBuffer, sizeof(aBuffer) / sizeof(*aBuffer));
        printf("(%d) %.*s\n", i, length, aBuffer);
    }

    ketl_bytecode bytecode = ketl_bytecode_compile(pState, ir, pState->pAllocator);
    ketl_free(pState->pAllocator, ir.pNodes);
    ketl_free(pState->pAllocator, ir.pSymbols);

    for (uint32_t i = 0u; i < bytecode.instructionsCount; 
            i += ketl_bytecode_decode_instruction_length(bytecode.pInstructions[i])) {
        char aBuffer[256];
        uint32_t length = ketl_bytecode_format(bytecode.pInstructions + i, bytecode.pLabels, aBuffer, sizeof(aBuffer) / sizeof(*aBuffer));
        printf("%d: %.*s\n", i, length, aBuffer);
    }

    uint32_t opcodesSize = 0u;
    uint8_t* pOpcodes = ketl_assembler_compile(bytecode, &opcodesSize, pState->pAllocator);
    ketl_free(pState->pAllocator, bytecode.pInstructions);

    {
        char aBuffer[1024];
        uint32_t length = ketl_assembler_format(pOpcodes, opcodesSize, aBuffer, sizeof(aBuffer) / sizeof(*aBuffer));
        printf("%.*s\n", length, aBuffer);
    }

    ketl_executable_memory ex_memory;
    ketl_executable_memory_init(&ex_memory, pState->pAllocator);

    uint8_t* executableOpcodes = ketl_executable_memory_allocate(&ex_memory, pOpcodes, opcodesSize);
    ketl_free(pState->pAllocator, pOpcodes);
    int64_t result = ketl_execute(&executableOpcodes);

    ketl_executable_memory_deinit(&ex_memory);

    return result;
}

ketl_type* ketl_state_find_type(const char* pName) {
    (void)pName;
    return NULL;
}
