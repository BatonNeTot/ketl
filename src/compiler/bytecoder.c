//🫖ketl

#include "bytecoder.h"

#include "ketl_impl.h"
#include "type_impl.h"
#include "namespace.h"

#include "containers/vector.h"
#include "containers/hash_map.h"

#include <stdlib.h>
#include <stdio.h>

KETL_VECTOR_DECLARATION(instructions, uint8_t)
KETL_VECTOR_DEFINITION(instructions, uint8_t)

KETL_DEFINE(arg_info) {
    ketl_type* pType;
    ketl_bytecode_stack_offset stackOffset;
};

KETL_HASH_MAP_DECLARATION(variables, const char*, arg_info)
KETL_HASH_MAP_DEFINITION(variables, const char*, arg_info, KETL_HASH_DEFAULT, KETL_EQUAL_DEFAULT)

KETL_DEFINE(bytecoder_context) {
    instructions vInstructions;
    variables mVariables;
    ketl_state* pState;
    char* pSymbols;
    ketl_bytecode_stack_offset usedStack;
};

#define PUSH_CONSTANT(instructions, value) (instructions_push_back_ref_n((instructions), (uint8_t*)&(value), sizeof(value)))

static arg_info get_arg_stack_offset(bytecoder_context* pContext, const char* symbol) {
    switch(symbol[0]) {
        case '#': {
            ketl_atomic_string sIntTypeName = ketl_atomic_strings_get(&pContext->pState->atomicStrings, "i64", 3);
            ketl_namespace_node* pTypeNode = ketl_namespace_find(&pContext->pState->globalNamespace, sIntTypeName);
            assert(pTypeNode->value.type == KETL_NAMESPACE_VALUE_TYPE && pTypeNode->nextOffset == (uint32_t)(-1));

            int64_t value = strtoll(symbol + 1, NULL, 10);

            ketl_bytecode_stack_offset stackOffset = pContext->usedStack;
            pContext->usedStack = stackOffset + sizeof(value);

            instructions_push_back_copy(&pContext->vInstructions, KETL_BYTECODE_64LOAD_CONST);
            PUSH_CONSTANT(&pContext->vInstructions, stackOffset);
            PUSH_CONSTANT(&pContext->vInstructions, value);
            return (arg_info){
                .pType = pTypeNode->value.pType,
                .stackOffset = stackOffset
            };
        }
        case '~': {
            arg_info argInfo = {
                .pType = NULL,
                .stackOffset = pContext->usedStack
            };
            variables_bucket* bucket = variables_get_or_insert_copy(&pContext->mVariables, symbol, argInfo);
            if (bucket->value.stackOffset == argInfo.stackOffset) {
                // TODO FIX take into account type size and alignment
                pContext->usedStack += sizeof(int64_t);
            }
            return bucket->value;
        }
        default: {
            variables_bucket* bucket = variables_get_or_null(&pContext->mVariables, symbol);
            if (bucket != NULL) {
                return bucket->value;
            }

            ketl_atomic_string sSymbol = ketl_atomic_strings_get(&pContext->pState->atomicStrings, symbol, KETL_NULL_TERMINATED_LENGTH_32);
            ketl_namespace_node* pSymbolNode = ketl_namespace_find(&pContext->pState->globalNamespace, sSymbol);
            if (pSymbolNode != NULL) {
                KETL_FOREVER {
                    if (pSymbolNode->value.type == KETL_NAMESPACE_VALUE_VAR) {
                        arg_info argInfo = {
                            .pType = pSymbolNode->value.var.pType,
                            .stackOffset = pContext->usedStack
                        };
                        variables_get_or_insert_copy(&pContext->mVariables, symbol, argInfo);
                        // TODO FIX take into account type size and alignment
                        pContext->usedStack += sizeof(int64_t);

                        // TODO FIX
                        // get type size and use appropriate load global bytecode
                        instructions_push_back_copy(&pContext->vInstructions, KETL_BYTECODE_64LOAD_CONST);
                        PUSH_CONSTANT(&pContext->vInstructions, argInfo.stackOffset);
                        PUSH_CONSTANT(&pContext->vInstructions, pSymbolNode->value.var.pValue);
                        return argInfo;
                    }

                    if (pSymbolNode->nextOffset == (uint32_t)(-1)) {
                        break;
                    }
                    pSymbolNode = pContext->pState->globalNamespace.vNodes.pData + pSymbolNode->nextOffset;
                }
            }

            // TODO ERROR
            printf("unknown variable %s\n", symbol);
            assert(false);
        }
    }
    return (arg_info){
        .pType = NULL,
        .stackOffset = 0
    };
}

static operator_overloading_map_bucket* determine_operator_binary(bytecoder_context* pContext, ketl_ir_type operatorType, arg_info args[2]) {
    // TODO FIX
    // for now we just hash search exact function, later we should take into acount possible implicit casts

    // first type is return type, ignored during search
    ketl_type_parameter parametersArray[] = { {.pType = NULL}, {.pType = args[0].pType}, {.pType = args[1].pType} };
    function_parameters parameters = {
        .pParameters = parametersArray,
        .parametersCount = sizeof(parametersArray) / sizeof(*parametersArray)
    };

    return operator_overloading_map_get_or_null(pContext->pState->amOperatorOverloading + (operatorType - KETL_IR_FIRST_OPERATOR), parameters);
}

static void add_footer(bytecoder_context* pContext, uint32_t stackReserveBackpatchOffset, ketl_bytecode_instr returnInstr) {
    ketl_bytecode_stack_offset stackReservedSize = pContext->usedStack;
    instructions_push_back_copy(&pContext->vInstructions, returnInstr);
    PUSH_CONSTANT(&pContext->vInstructions, stackReservedSize);
    *(ketl_bytecode_stack_offset*)(pContext->vInstructions.pData + stackReserveBackpatchOffset) = stackReservedSize;
}

static ketl_bytecode create_bytecode_struct(bytecoder_context* pContext) {
    variables_deinit(&pContext->mVariables);

    return (ketl_bytecode){
        .pInstructions = pContext->vInstructions.pData,
        .instructionsCount = pContext->vInstructions.size,
    };
}

ketl_bytecode ketl_bytecode_compile(ketl_state* pState, ketl_ir ir, const ketl_allocator* pAllocator) {
    bytecoder_context context = {
        .pState = pState,
        .pSymbols = ir.pSymbols,
        .usedStack = 0,
    };
    instructions_init(&context.vInstructions, 16, pAllocator);
    variables_init(&context.mVariables, pAllocator);
    
    ketl_bytecode_stack_offset stackReserveDummy = 0;
    instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_STACK_PROLOG);
    uint32_t stackReserveBackpatchOffset = context.vInstructions.size;
    PUSH_CONSTANT(&context.vInstructions, stackReserveDummy);

    for (uint32_t i = 0u; i < ir.nodesCount; ++i) {
        ketl_ir_node node = ir.pNodes[i];
        switch(node.type) {
            case KETL_IR_TYPE_PUSH_ARGUMENT: {
                arg_info arg0 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[0]);

                // TODO FIX
                // get type size and use appropriate push arg bytecode
                instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_64PUSH_ARG);
                PUSH_CONSTANT(&context.vInstructions, arg0.stackOffset);
                break;
            }
            case KETL_IR_TYPE_CALL: {
                arg_info arg0 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[0]);
                arg_info arg1 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[1]);

                instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_CALL);
                PUSH_CONSTANT(&context.vInstructions, arg0.stackOffset);
                PUSH_CONSTANT(&context.vInstructions, arg1.stackOffset);
                break;
            }
            case KETL_IR_TYPE_RETURN: {
                add_footer(&context, stackReserveBackpatchOffset, KETL_BYTECODE_RETURN);
                return create_bytecode_struct(&context);
            }
            case KETL_IR_TYPE_RETURN_VALUE: {
                arg_info arg0 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[0]);
                
                // TODO FIX
                // get type size and use appropriate return bytecode
                add_footer(&context, stackReserveBackpatchOffset, KETL_BYTECODE_64RETURN);
                PUSH_CONSTANT(&context.vInstructions, arg0.stackOffset);
                return create_bytecode_struct(&context);
            }
            case KETL_IR_TYPE_PLUS:
            case KETL_IR_TYPE_MULTIPLY: {
                arg_info args[] = {
                    get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[0]),
                    get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[1]),
                    get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[2]),
                };

                operator_overloading_map_bucket* operatorBucket = determine_operator_binary(&context, node.type, 
                    // first arg is return target
                    args + 1);
                if (operatorBucket == NULL) {
                    assert(false); // TODO ERROR
                }
                
                // TODO FIX
                // check if return argument olready has defined type and do casting if necessary
                variables_bucket* bucket = variables_get_or_null(&context.mVariables, ir.pSymbols + node.aArgs[0]);
                bucket->value.pType = operatorBucket->key.pParameters[0].pType;
                
                instructions_push_back_copy(&context.vInstructions, operatorBucket->value);

                PUSH_CONSTANT(&context.vInstructions, args[0].stackOffset);
                PUSH_CONSTANT(&context.vInstructions, args[1].stackOffset);
                PUSH_CONSTANT(&context.vInstructions, args[2].stackOffset);
                break;
            }
            default: {
                // TODO ERROR
                char aBuffer[256];
                uint32_t length = ketl_ir_node_format(node, ir.pSymbols, aBuffer, sizeof(aBuffer) / sizeof(*aBuffer));
                printf("unknown ir node: %.*s\n", length, aBuffer);
                assert(false);
            }
        }
    }
    
    add_footer(&context, stackReserveBackpatchOffset, KETL_BYTECODE_RETURN);
    return create_bytecode_struct(&context);
}
