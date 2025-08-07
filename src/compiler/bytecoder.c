//🫖ketl

#include "bytecoder.h"

#include "ketl_impl.h"
#include "type_impl.h"
#include "namespace.h"

#include "containers/vector.h"
#include "containers/hash_map.h"
#include "str.h"

#include <stdlib.h>
#include <stdio.h>

KETL_VECTOR_DECLARATION(instructions, uint8_t)
KETL_VECTOR_DEFINITION(instructions, uint8_t)

KETL_DEFINE(arg_info) {
    ketl_type* pType;
    ketl_bytecode_stack_offset stackOffset;
};

KETL_DEFINE(undefined_value) {
    ketl_variable* pValue;
    undefined_value* pNextValue;
};

KETL_HASH_MAP_DECLARATION(variables, ketl_hir_var_id_t, arg_info)
KETL_HASH_MAP_DEFINITION(variables, ketl_hir_var_id_t, arg_info, KETL_HASH_DEFAULT, KETL_EQUAL_DEFAULT)

KETL_DEFINE(bytecoder_context) {
    instructions vInstructions;
    variables mVariables;
    ketl_state* pState;
    char* pSymbols;
    ketl_hir_t* p_hir;
    ketl_hir_var_id_t global_var;
    ketl_bytecode_stack_offset usedStack;
};

#define PUSH_CONSTANT(instructions, value) (instructions_push_back_ref_n((instructions), (uint8_t*)&(value), sizeof(value)))

static arg_info push_value_as_arg(bytecoder_context* pContext, ketl_hir_var_id_t var_id, ketl_variable* pValue) {
    arg_info argInfo = {
        .pType = pValue->pType,
        .stackOffset = pContext->usedStack
    };
    variables_get_or_insert_copy(&pContext->mVariables, var_id, argInfo);
    // TODO FIX take into account type size and alignment
    pContext->usedStack += sizeof(int64_t);

    // TODO FIX
    // get type size and use appropriate load global bytecode
    instructions_push_back_copy(&pContext->vInstructions, KETL_BYTECODE_64LOAD_CONST);
    PUSH_CONSTANT(&pContext->vInstructions, argInfo.stackOffset);
    PUSH_CONSTANT(&pContext->vInstructions, pValue->pointer);
    return argInfo;
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

static arg_info hir_get_arg_stack_offset(bytecoder_context* pContext, ketl_hir_var_id_t var_id) {
    ketl_hir_var_t var = pContext->p_hir->p_vars[var_id];
    if (var.uid == KETL_HIR_VAR_UID_LITERAL) {
        int64_t value = strtoll(KETL_ATOMIC_STRING_GET_POINTER(pContext->pSymbols, var.literal), NULL, 10);
        ketl_type* p_type = pContext->p_hir->p_used_types[var.type];

        ketl_bytecode_stack_offset stackOffset = pContext->usedStack;
        pContext->usedStack = stackOffset + sizeof(value);

        instructions_push_back_copy(&pContext->vInstructions, KETL_BYTECODE_64LOAD_CONST);
        PUSH_CONSTANT(&pContext->vInstructions, stackOffset);
        PUSH_CONSTANT(&pContext->vInstructions, value);
        return (arg_info){
            .pType = p_type,
            .stackOffset = stackOffset
        };
    }

    // local variable
    // might be existing temporart variable
    variables_bucket* bucket = variables_get_or_null(&pContext->mVariables, var_id);
    if (bucket != NULL) {
        return bucket->value;
    }

    // TODO ERROR
    const char* p_symbol = KETL_ATOMIC_STRING_GET_POINTER(pContext->pSymbols, pContext->p_hir->p_vars_infos[var.info].name);
    printf("unknown variable %s\n", p_symbol);
    KETL_ASSERT(false);
        
    return (arg_info){
        .pType = NULL,
        .stackOffset = 0
    };
}

static ketl_bytecode_instr get_binary_bytecode(ketl_hir_tag_t hir_tag) {
    KETL_SWITCH_STRICT (hir_tag) {
        case KETL_HIR_PLUS_I64:
            return KETL_BYTECODE_64IADD;
        case KETL_HIR_MINUS_I64:
            return KETL_BYTECODE_64ISUB;
        case KETL_HIR_MULTY_I64:
            return KETL_BYTECODE_64IMULTIPLY;
        case KETL_HIR_DIV_I64:
            return KETL_BYTECODE_64IDIVIDE;
        case KETL_HIR_MOD_I64:
            return KETL_BYTECODE_64IMODULO;

        case KETL_HIR_EQUAL_I64:
            return KETL_BYTECODE_64IEQUAL;
        case KETL_HIR_NOT_EQUAL_I64:
            return KETL_BYTECODE_64INOT_EQUAL;
        case KETL_HIR_LESS_I64:
            return KETL_BYTECODE_64ILESS;
        case KETL_HIR_LESS_OR_EQUAL_I64:
            return KETL_BYTECODE_64ILESS_OR_EQUAL;
        case KETL_HIR_GREATER_I64:
            return KETL_BYTECODE_64IGREATER;
        case KETL_HIR_GREATER_OR_EQUAL_I64:
            return KETL_BYTECODE_64IGREATER_OR_EQUAL;
    }
} 

ketl_bytecode ketl_bytecode_compile_from_hir(ketl_state* pState, ketl_hir_t* p_hir, const ketl_allocator* p_allocator) {
    bytecoder_context context = {
        .pState = pState,
        .pSymbols = p_hir->p_symbols,
        .p_hir = p_hir,
        .global_var = (ketl_hir_var_id_t)-1,
        .usedStack = 0,
    };
    instructions_init(&context.vInstructions, 16, p_allocator);
    variables_init(&context.mVariables, p_allocator);
    
    ketl_bytecode_stack_offset stackReserveDummy = 0;
    instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_STACK_PROLOG);
    uint32_t stackReserveBackpatchOffset = context.vInstructions.size;
    PUSH_CONSTANT(&context.vInstructions, stackReserveDummy);

    for (ketl_hir_var_id_t var_id = 0u; var_id < p_hir->vars_count; ++var_id) {
        ketl_hir_var_t var = p_hir->p_vars[var_id];
        if (var.uid == KETL_HIR_VAR_UID_LITERAL) {
            continue;
        }

        if (var.info != KETL_HIR_VAR_INFO_TEMP &&
            p_hir->p_vars_infos[var.info].p_global != NULL) {
            push_value_as_arg(&context, var_id, p_hir->p_vars_infos[var.info].p_global);
            continue;
        }

        //KETL_ASSERT(var.type != KETL_HIR_USED_TYPE_UNKHOWN);
        // temprorary variable
        arg_info argInfo = {
            // TODO FIX
            .pType = var.type != KETL_HIR_USED_TYPE_UNKHOWN ? p_hir->p_used_types[var.type] : NULL,
            .stackOffset = context.usedStack
        };

        variables_bucket* bucket = variables_get_or_insert_copy(&context.mVariables, var_id, argInfo);
        if (bucket->value.stackOffset == argInfo.stackOffset) {
            // TODO FIX take into account type size and alignment
            context.usedStack += sizeof(int64_t);
        }
    }

    for (uint32_t i = 0u; i < p_hir->instrs_count; i += ketl_hir_decode_size(p_hir, i)) {
        uint8_t* p_instr = p_hir->p_instrs + i;

        ketl_hir_header_t header = *(ketl_hir_header_t*)p_instr;
        p_instr += sizeof(ketl_hir_header_t);

        KETL_SWITCH_STRICT (header.tag) {
            case KETL_HIR_PLUS_I64:
            case KETL_HIR_MINUS_I64:
            case KETL_HIR_MULTY_I64:
            case KETL_HIR_DIV_I64:
            case KETL_HIR_MOD_I64:

            case KETL_HIR_EQUAL_I64:
            case KETL_HIR_NOT_EQUAL_I64:
            case KETL_HIR_LESS_I64:
            case KETL_HIR_LESS_OR_EQUAL_I64:
            case KETL_HIR_GREATER_I64:
            case KETL_HIR_GREATER_OR_EQUAL_I64: {
                ketl_hir_binary_op_t* p_hir_info = (ketl_hir_binary_op_t*)p_instr;

                arg_info args[] = {
                    hir_get_arg_stack_offset(&context, p_hir_info->output_var),
                    hir_get_arg_stack_offset(&context, p_hir_info->lhs_var),
                    hir_get_arg_stack_offset(&context, p_hir_info->rhs_var),
                };
                
                instructions_push_back_copy(&context.vInstructions, get_binary_bytecode(header.tag));

                PUSH_CONSTANT(&context.vInstructions, args[0].stackOffset);
                PUSH_CONSTANT(&context.vInstructions, args[1].stackOffset);
                PUSH_CONSTANT(&context.vInstructions, args[2].stackOffset);
                break;
            }
            case KETL_HIR_CALL: {
                ketl_hir_call_t* p_hir_info = (ketl_hir_call_t*)p_instr;

                arg_info output_arg = hir_get_arg_stack_offset(&context, p_hir_info->output_var);
                arg_info callee_arg = hir_get_arg_stack_offset(&context, p_hir_info->callee);

                // TODO FIX allow other types to be called
                KETL_ASSERT(callee_arg.pType->type == KETL_TYPE_CFUNCTION);

                for (uint32_t i = 0u; i < p_hir_info->arguments_count; ++i) {
                    arg_info arg = hir_get_arg_stack_offset(&context, p_hir_info->arguments[i]);

                    // TODO FIX
                    // get type size and use appropriate push arg bytecode
                    instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_64PUSH_ARG);
                    PUSH_CONSTANT(&context.vInstructions, arg.stackOffset);
                }

                instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_CALL);
                PUSH_CONSTANT(&context.vInstructions, output_arg.stackOffset);
                PUSH_CONSTANT(&context.vInstructions, callee_arg.stackOffset);
                break;
            }
            case KETL_HIR_ASSIGN: {
                
                ketl_hir_assign_t* p_hir_info = (ketl_hir_assign_t*)p_instr;

                arg_info args[] = {
                    hir_get_arg_stack_offset(&context, p_hir_info->dest_var),
                    hir_get_arg_stack_offset(&context, p_hir_info->source_var),
                };
                
                // TODO FIX
                // get type size and use appropriate push arg bytecode
                instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_64ASSIGN);

                PUSH_CONSTANT(&context.vInstructions, args[0].stackOffset);
                PUSH_CONSTANT(&context.vInstructions, args[1].stackOffset);
                break;
            }
            case KETL_HIR_RETURN_VALUE: {
                ketl_hir_return_value_t* p_hir_info = (ketl_hir_return_value_t*)p_instr;

                arg_info arg0 = hir_get_arg_stack_offset(&context, p_hir_info->value_var);
                
                // TODO FIX
                // get type size and use appropriate return bytecode
                add_footer(&context, stackReserveBackpatchOffset, KETL_BYTECODE_64RETURN);
                PUSH_CONSTANT(&context.vInstructions, arg0.stackOffset);
                return create_bytecode_struct(&context);
                break;
            }
        }
    }
    
    add_footer(&context, stackReserveBackpatchOffset, KETL_BYTECODE_RETURN);
    return create_bytecode_struct(&context);
}
