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

#define PARAMETERS_HASH(key) parameters_hash(&(key))

static uint64_t parameters_hash(const function_parameters* pParameters) {
    uint64_t hash = 0u;
    uint16_t parametersCount = pParameters->parametersCount;
    for (uint16_t i = 0u; i < parametersCount; ++i) {
        hash = ((uint64_t)pParameters->pParameters[i].pType) ^ (hash << 1);
    }
    return hash;
}

#define IS_PARAMETERS_EQUAL(lhsKey, rhsKey) is_parameters_equal(&(lhsKey), &(rhsKey))

static bool is_parameters_equal(const function_parameters* pLhsParameters, const function_parameters* pRhsParameters) {
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

KETL_NAMED_HASH_MAP_DEFINITION(function_types_map, function_parameters, ketl_type_function*, PARAMETERS_HASH, IS_PARAMETERS_EQUAL)

KETL_NAMED_VECTOR_DEFINITION(function_meta_types, ketl_type_meta*)

KETL_NAMED_HASH_MAP_DEFINITION(operator_overloading_map, function_parameters, ketl_bytecode_instr, PARAMETERS_HASH, IS_PARAMETERS_EQUAL)

static ketl_type_function* get_function_type(ketl_state* pState, const function_parameters* pParameters) {
    uint16_t parametersCount = pParameters->parametersCount;
    function_types_map_bucket* pBucket = function_types_map_get_or_insert_copy(&pState->mFunctionTypes, *pParameters, NULL);
    if (pBucket->value == NULL) {
        while (pState->vFunctionMetaTypes.size <= parametersCount) {
            ketl_type_meta* pMeta = ketl_gc_create(&pState->gc, pState->pMainMetaType, KETL_GC_ROOT);
            *pMeta = (ketl_type_meta){
                .pName = "",
                .type = KETL_TYPE_META,
                .align = _Alignof(ketl_type_function),
                .size = sizeof(ketl_type_function) + pState->vFunctionMetaTypes.size * sizeof(ketl_type_parameter)
            };
            function_meta_types_push_back_copy(&pState->vFunctionMetaTypes, pMeta);
        }

        ketl_type_meta* pMeta = pState->vFunctionMetaTypes.pData[parametersCount - 1];
        ketl_type_function* pFunction = ketl_gc_create(&pState->gc, (ketl_type*)pMeta, KETL_GC_ROOT);
        *pFunction = (ketl_type_function){
            .pName = "",
            .type = KETL_TYPE_META,
            .align = _Alignof(void(*)()),
            .size = sizeof(void(*)()),
            .parametersCount = parametersCount
        };
        ketl_memcpy(pFunction->aParameters, pParameters->pParameters, parametersCount * sizeof(ketl_type_parameter));

        pBucket->key.pParameters = pFunction->aParameters;
        pBucket->value = pFunction;
    }
    return pBucket->value;
}

ketl_state* ketl_state_create(const ketl_allocator* pAllocator) {
    ketl_state* pState = ketl_alloc(pAllocator, sizeof(ketl_state));
    *pState = (ketl_state){
        .pAllocator = pAllocator
    };

    ketl_gc_init(&pState->gc, pAllocator);
    ketl_atomic_strings_init(&pState->atomicStrings, pAllocator);
    function_types_map_init(&pState->mFunctionTypes, pAllocator);
    function_meta_types_init(&pState->vFunctionMetaTypes, 4, pAllocator);
    ketl_memset(pState->vFunctionMetaTypes.pData, 0, 4 * sizeof(ketl_type_meta*));

    for (uint32_t i = 0; i < (sizeof(pState->amOperatorOverloading) / sizeof(*pState->amOperatorOverloading)); ++i) {
        operator_overloading_map_init(pState->amOperatorOverloading + i, pAllocator);
    }

#define INIT_TYPE(_var, _type) *(_type*)(_var) = (_type)

    ketl_type* pMainMetaType = pState->pMainMetaType = ketl_alloc(pAllocator, sizeof(ketl_type_meta));
    INIT_TYPE(pMainMetaType, ketl_type_meta) {
        .pName = "",
        .type = KETL_TYPE_META,
        .align = _Alignof(ketl_type_meta),
        .size = sizeof(ketl_type_meta)
    };
    ketl_gc_reg(&pState->gc, pMainMetaType, pMainMetaType, KETL_GC_ROOT | KETL_GC_FREE_AFTER_USE);

    ketl_type* pPrimitiveMetaType = ketl_gc_create(&pState->gc, pMainMetaType, KETL_GC_ROOT);
    INIT_TYPE(pPrimitiveMetaType, ketl_type_meta) {
        .pName = "",
        .type = KETL_TYPE_META,
        .align = _Alignof(ketl_type_primitive),
        .size = sizeof(ketl_type_primitive)
    };

#define CREATE_PRIMITIVE_TYPE(_varName, _name, _size, _isInteger, _isSigned)\
ketl_type* _varName = ketl_gc_create(&pState->gc, pPrimitiveMetaType, KETL_GC_ROOT); \
INIT_TYPE(_varName, ketl_type_primitive) {\
        .pName = _name,\
        .type = KETL_TYPE_PRIMITIVE,\
        .align = _size,\
        .size = _size,\
        .isInteger = _isInteger,\
        .isSigned = _isSigned,\
        }

    CREATE_PRIMITIVE_TYPE(tVoid, "void", 0, false, false);
    CREATE_PRIMITIVE_TYPE(tInt64, "i64", 8, true, true);

    {
        ketl_type_parameter parametersArray[] = { {.pType = tInt64}, {.pType = tInt64}, {.pType = tInt64} };
        function_parameters parameters = {
            .pParameters = parametersArray,
            .parametersCount = sizeof(parametersArray) / sizeof(*parametersArray)
        };

        ketl_type_function* pFunctionType = get_function_type(pState, &parameters);
        parameters.pParameters = pFunctionType->aParameters;

        operator_overloading_map_get_or_insert_copy(pState->amOperatorOverloading + (KETL_IR_TYPE_PLUS - KETL_IR_FIRST_OPERATOR), parameters, KETL_BYTECODE_I64ADD);
    }

    return pState;
}

void ketl_state_destroy(ketl_state* pState) {
    for (uint32_t i = 0; i < (sizeof(pState->amOperatorOverloading) / sizeof(*pState->amOperatorOverloading)); ++i) {
        operator_overloading_map_deinit(pState->amOperatorOverloading + i);
    }
    function_meta_types_deinit(&pState->vFunctionMetaTypes);
    function_types_map_deinit(&pState->mFunctionTypes);
    ketl_atomic_strings_deinit(&pState->atomicStrings);
    ketl_gc_deinit(&pState->gc);

    ketl_free(pState->pAllocator, pState);
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

    ketl_bytecode bytecode = ketl_bytecode_compile(ir, pState->pAllocator);
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
