//🫖ketl

#include "bytecoder.h"

#include "type_impl.h"

#include "containers/vector.h"
#include "containers/hash_map.h"

#include <stdlib.h>

KETL_NAMED_VECTOR_DECLARATION(instructions, uint8_t)
KETL_NAMED_VECTOR_DEFINITION(instructions, uint8_t)

KETL_DEFINE(arg_info) {
    ketl_type* pType;
    ketl_bytecode_stack_offset stackOffset;
};

KETL_NAMED_HASH_MAP_DECLARATION(variables, const char*, arg_info)
KETL_NAMED_HASH_MAP_DEFINITION(variables, const char*, arg_info, KETL_HASH_DEFAULT, KETL_EQUAL_DEFAULT)

KETL_DEFINE(bytecoder_context) {
    instructions vInstructions;
    variables mVariables;
    char* pSymbols;
    ketl_bytecode_stack_offset usedStack;
};

static arg_info get_arg_stack_offset(bytecoder_context* pContext, const char* symbol) {
    switch(symbol[0]) {
        case '#': {
            int64_t value = strtoll(symbol + 1, NULL, 10);

            ketl_bytecode_stack_offset stackOffset = pContext->usedStack;
            pContext->usedStack = stackOffset + sizeof(value);

            instructions_push_back_copy(&pContext->vInstructions, KETL_BYTECODE_I64LOAD_CONST);
            instructions_push_back_ref_n(&pContext->vInstructions, (uint8_t*)&stackOffset, sizeof(stackOffset));
            instructions_push_back_ref_n(&pContext->vInstructions, (uint8_t*)&value, sizeof(value));
            return (arg_info){
                .pType = NULL,
                .stackOffset = stackOffset
            };
        }
        default: {
            arg_info argInfo = {
                .pType = NULL,
                .stackOffset = pContext->usedStack
            };
            variables_bucket* bucket = variables_get_or_insert_copy(&pContext->mVariables, symbol, argInfo);
            if (bucket->value.stackOffset == argInfo.stackOffset) {
                pContext->usedStack += sizeof(int64_t);
            }
            return bucket->value;
        }
    }
    return (arg_info){
        .pType = NULL,
        .stackOffset = 0
    };
}

void add_footer(bytecoder_context* pContext, uint32_t stackReserveBackpatchOffset, ketl_bytecode_instr returnInstr) {
    ketl_bytecode_stack_offset stackReservedSize = pContext->usedStack;
    instructions_push_back_copy(&pContext->vInstructions, returnInstr);
    instructions_push_back_ref_n(&pContext->vInstructions, (uint8_t*)&stackReservedSize, sizeof(stackReservedSize));
    *(ketl_bytecode_stack_offset*)(pContext->vInstructions.pData + stackReserveBackpatchOffset) = stackReservedSize;
}

ketl_bytecode create_bytecode_struct(bytecoder_context* pContext) {
    variables_deinit(&pContext->mVariables);

    return (ketl_bytecode){
        .pInstructions = pContext->vInstructions.pData,
        .instructionsCount = pContext->vInstructions.size,
    };
}

ketl_bytecode ketl_bytecode_compile(ketl_ir ir, const ketl_allocator* pAllocator) {
    bytecoder_context context = {
        .pSymbols = ir.pSymbols,
        .usedStack = 0,
    };
    instructions_init(&context.vInstructions, 16, pAllocator);
    variables_init(&context.mVariables, pAllocator);
    
    ketl_bytecode_stack_offset stackReserveDummy = 0;
    instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_STACK_PROLOG);
    uint32_t stackReserveBackpatchOffset = context.vInstructions.size;
    instructions_push_back_ref_n(&context.vInstructions, (uint8_t*)&stackReserveDummy, sizeof(stackReserveDummy));

    for (uint32_t i = 0u; i < ir.nodesCount; ++i) {
        ketl_ir_node node = ir.pNodes[i];
        switch(node.type) {
            case KETL_IR_TYPE_PLUS: {
                arg_info arg0 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[0]);
                arg_info arg1 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[1]);
                arg_info arg2 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[2]);

                instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_I64ADD);
                instructions_push_back_ref_n(&context.vInstructions, (uint8_t*)&arg0.stackOffset, sizeof(arg0));
                instructions_push_back_ref_n(&context.vInstructions, (uint8_t*)&arg1.stackOffset, sizeof(arg1));
                instructions_push_back_ref_n(&context.vInstructions, (uint8_t*)&arg2.stackOffset, sizeof(arg2));
                break;
            }
            case KETL_IR_TYPE_MULTIPLY: {
                arg_info arg0 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[0]);
                arg_info arg1 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[1]);
                arg_info arg2 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[2]);

                instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_I64MULTIPLY);
                instructions_push_back_ref_n(&context.vInstructions, (uint8_t*)&arg0.stackOffset, sizeof(arg0));
                instructions_push_back_ref_n(&context.vInstructions, (uint8_t*)&arg1.stackOffset, sizeof(arg1));
                instructions_push_back_ref_n(&context.vInstructions, (uint8_t*)&arg2.stackOffset, sizeof(arg2));
                break;
            }
            case KETL_IR_TYPE_RETURN_VALUE: {
                arg_info arg0 = get_arg_stack_offset(&context, ir.pSymbols + node.aArgs[0]);
                
                add_footer(&context, stackReserveBackpatchOffset, KETL_BYTECODE_RETURN_64VALUE);
                instructions_push_back_ref_n(&context.vInstructions, (uint8_t*)&arg0.stackOffset, sizeof(arg0));
                return create_bytecode_struct(&context);
            }
            case KETL_IR_TYPE_RETURN: {
                add_footer(&context, stackReserveBackpatchOffset, KETL_BYTECODE_RETURN);
                return create_bytecode_struct(&context);
            }
        }
    }
    
    add_footer(&context, stackReserveBackpatchOffset, KETL_BYTECODE_RETURN);
    return create_bytecode_struct(&context);
}
