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

KETL_HASH_MAP_DECLARATION(instr_to_bytecode_offsets, ketl_hir_instr_offset_t, uint32_t)
KETL_HASH_MAP_DEFINITION(instr_to_bytecode_offsets, ketl_hir_instr_offset_t, uint32_t, ANN_HASH, ANN_EQUAL)

ANN_DEFINE(arg_info) {
    ketl_type* p_type;
    ketl_bytecode_stack_offset stackOffset;
};

ANN_DEFINE(undefined_value) {
    ketl_variable* pValue;
    undefined_value* pNextValue;
};

KETL_HASH_MAP_DECLARATION(variables, ketl_hir_var_id_t, arg_info)
KETL_HASH_MAP_DEFINITION(variables, ketl_hir_var_id_t, arg_info, ANN_HASH, ANN_EQUAL)

ANN_DEFINE(bytecoder_context) {
    instructions vInstructions;
    instr_to_bytecode_offsets m_instr_to_bytecode_offsets;
    variables mVariables;
    ketl_state* pState;
    char* pSymbols;
    ketl_hir_t* p_hir;
    ketl_hir_var_id_t global_var;
    ketl_bytecode_stack_offset usedStack;
};

#define PUSH_CONSTANT(instructions, value) (instructions_push_back_ref_n((instructions), (uint8_t*)&(value), sizeof(value)))

static arg_info hir_get_arg_stack_offset(bytecoder_context* pContext, ketl_hir_var_id_t var_id) {
    ketl_hir_var_t var = pContext->p_hir->p_vars[var_id];

    // local variable
    // might be existing temporart variable
    variables_bucket* bucket = variables_get_or_null(&pContext->mVariables, var_id);
    if (bucket != NULL) {
        return bucket->value;
    }

    // TODO ERROR
    const char* p_symbol = KETL_ATOMIC_STRING_GET_POINTER(pContext->pSymbols, pContext->p_hir->p_vars_infos[var.info].name);
    printf("unknown variable %s\n", p_symbol);
    ANN_ASSERT(false);
        
    return (arg_info){
        .p_type = NULL,
        .stackOffset = 0
    };
}

static ketl_bytecode_instr get_binary_bytecode(ketl_hir_tag_t hir_tag) {
    ANN_SWITCH_STRICT (hir_tag & KETL_HIR_TYPE_INSTR_MASK) {
        case KETL_HIR_PLUS:
            return KETL_BYTECODE_8UADD + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);
        case KETL_HIR_MINUS:
            return KETL_BYTECODE_8USUB + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);
        case KETL_HIR_MULTY:
            return KETL_BYTECODE_8UMULTIPLY + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);
        case KETL_HIR_DIV:
            return KETL_BYTECODE_8UDIVIDE + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);
        case KETL_HIR_MOD:
            return KETL_BYTECODE_8UMODULO + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);

        case KETL_HIR_EQUAL:
            return KETL_BYTECODE_8UEQUAL + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);
        case KETL_HIR_NOT_EQUAL:
            return KETL_BYTECODE_8UNOT_EQUAL + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);
        case KETL_HIR_LESS:
            return KETL_BYTECODE_8ULESS + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);
        case KETL_HIR_LESS_OR_EQUAL:
            return KETL_BYTECODE_8ULESS_OR_EQUAL + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);
        case KETL_HIR_GREATER:
            return KETL_BYTECODE_8UGREATER + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);
        case KETL_HIR_GREATER_OR_EQUAL:
            return KETL_BYTECODE_8UGREATER_OR_EQUAL + ((hir_tag & KETL_HIR_TYPE_MASK) - KETL_HIR_U8);
    }
} 

static ketl_hir_block_index_t get_first_non_empty_block(ketl_hir_t* p_hir, ketl_hir_block_index_t block) {
    ANN_FOREVER {
        ketl_hir_instr_offset_t instr_offset = p_hir->p_block_offsets[block];
        uint8_t* p_instr = p_hir->p_instrs + instr_offset;
        ketl_hir_header_t* p_header = (ketl_hir_header_t*)p_instr;
        if (p_header->tag != KETL_HIR_JUMP) {
            return block;
        }
        ketl_hir_jump_t* p_jump = (ketl_hir_jump_t*)(p_instr + sizeof(ketl_hir_header_t));
        block = p_jump->block_index;
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
    instr_to_bytecode_offsets_init(&context.m_instr_to_bytecode_offsets, p_allocator);
    variables_init(&context.mVariables, p_allocator);
    
    ketl_bytecode_stack_offset stackReserveDummy = 0;
    instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_STACK_PROLOG);
    uint32_t stackReserveBackpatchOffset = context.vInstructions.size;
    PUSH_CONSTANT(&context.vInstructions, stackReserveDummy);

    ketl_bytecode_stack_offset stackReservedSize = 0u;
    for (ketl_hir_var_id_t var_id = 0u; var_id < p_hir->vars_count; ++var_id) {
        ketl_hir_var_t var = p_hir->p_vars[var_id];
        if (var.uid == KETL_HIR_VAR_UID_LITERAL) {
	    const char* p_literal = KETL_ATOMIC_STRING_GET_POINTER(context.pSymbols, var.literal);
	    ANN_ASSERT(p_literal != NULL);
            int64_t value = strtoll(p_literal, NULL, 10);
            ketl_type* p_type = context.p_hir->p_used_types[var.type];

            arg_info argInfo = {
                .p_type = p_type,
                .stackOffset = stackReservedSize
            };
            variables_get_or_insert_copy(&context.mVariables, var_id, argInfo);
            // TODO FIX take into account type size and alignment
            stackReservedSize += sizeof(int64_t);

            // TODO FIX
            // get type size and use appropriate load global bytecode
            instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_64LOAD_CONST);
            PUSH_CONSTANT(&context.vInstructions, argInfo.stackOffset);
            PUSH_CONSTANT(&context.vInstructions, value);
            continue;
        }

        if (var.info != KETL_HIR_VAR_INFO_TEMP &&
            p_hir->p_vars_infos[var.info].p_global != NULL) {
            ketl_variable* p_value = p_hir->p_vars_infos[var.info].p_global;

            arg_info argInfo = {
                .p_type = p_value->p_type,
                .stackOffset = stackReservedSize
            };
            variables_get_or_insert_copy(&context.mVariables, var_id, argInfo);
            // TODO FIX take into account type size and alignment
            stackReservedSize += sizeof(int64_t);

            // TODO FIX
            // get type size and use appropriate load global bytecode
            instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_64LOAD_CONST);
            PUSH_CONSTANT(&context.vInstructions, argInfo.stackOffset);
            PUSH_CONSTANT(&context.vInstructions, p_value->pointer);
            continue;
        }

        // temprorary variable
        arg_info argInfo = {
            .p_type = var.type != KETL_HIR_USED_TYPE_UNKNOWN ? p_hir->p_used_types[var.type] : NULL,
            .stackOffset = stackReservedSize
        };

        variables_bucket* bucket = variables_get_or_insert_copy(&context.mVariables, var_id, argInfo);
        if (bucket->value.stackOffset == argInfo.stackOffset) {
            // TODO FIX take into account type size and alignment
            stackReservedSize += sizeof(int64_t);
        }
    }
    
    *(ketl_bytecode_stack_offset*)(context.vInstructions.p_data + stackReserveBackpatchOffset) = stackReservedSize;

    bool first_instr_in_block = true;

    for (uint32_t i = 0u; i < p_hir->instrs_count; i += ketl_hir_decode_size(p_hir, i)) {
        if (first_instr_in_block) {
            instr_to_bytecode_offsets_get_or_insert_copy(&context.m_instr_to_bytecode_offsets, i, context.vInstructions.size);
            first_instr_in_block = false;
        }

        uint8_t* p_instr = p_hir->p_instrs + i;

        ketl_hir_header_t header = *(ketl_hir_header_t*)p_instr;
        p_instr += sizeof(ketl_hir_header_t);

        switch (header.tag & KETL_HIR_TYPE_INSTR_MASK) {
            case KETL_HIR_PLUS:
            case KETL_HIR_MINUS:
            case KETL_HIR_MULTY:
            case KETL_HIR_DIV:
            case KETL_HIR_MOD:

            case KETL_HIR_EQUAL:
            case KETL_HIR_NOT_EQUAL:
            case KETL_HIR_LESS:
            case KETL_HIR_LESS_OR_EQUAL:
            case KETL_HIR_GREATER:
            case KETL_HIR_GREATER_OR_EQUAL: {
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

            default:
            ANN_SWITCH_STRICT (header.tag) {
                case KETL_HIR_CALL: {
                    ketl_hir_call_t* p_hir_info = (ketl_hir_call_t*)p_instr;

                    arg_info output_arg = hir_get_arg_stack_offset(&context, p_hir_info->output_var);
                    arg_info callee_arg = hir_get_arg_stack_offset(&context, p_hir_info->callee);

                    // TODO FIX allow other types to be called
                    ANN_ASSERT(callee_arg.p_type->type == KETL_TYPE_CFUNCTION);

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
                case KETL_HIR_JUMP: {
                    ketl_hir_jump_t* p_hir_info = (ketl_hir_jump_t*)p_instr;

                    if (!first_instr_in_block) {
                        ketl_hir_block_index_t dest = get_first_non_empty_block(p_hir, p_hir_info->block_index);
                        
                        if (p_hir->p_block_offsets[dest] != i + ketl_hir_decode_size(p_hir, i)) {
                            instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_JUMP);

                            _Static_assert(sizeof(ketl_bytecode_jump_offset) == sizeof(ketl_hir_block_index_t), "");
                            ketl_bytecode_jump_offset placeholder = dest;
                            PUSH_CONSTANT(&context.vInstructions, placeholder);
                        }
                    }
                    
                    first_instr_in_block = true;
                    break;
                }
                case KETL_HIR_JUMP_IF_TRUE: {
                    ketl_hir_jump_if_t* p_hir_info = (ketl_hir_jump_if_t*)p_instr;

                    ketl_hir_block_index_t true_dest = get_first_non_empty_block(p_hir, p_hir_info->true_block);
                    ketl_hir_block_index_t false_dest = get_first_non_empty_block(p_hir, p_hir_info->false_block);
                    arg_info expr = hir_get_arg_stack_offset(&context, p_hir_info->expr_var);
                    
                    // TODO FIX
                    // get type size and use appropriate return bytecode
                    instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_64JUMP_IF);

                    _Static_assert(sizeof(ketl_bytecode_jump_offset) == sizeof(ketl_hir_block_index_t), "");
                    ketl_bytecode_jump_offset placeholder = true_dest;
                    PUSH_CONSTANT(&context.vInstructions, placeholder);
                    PUSH_CONSTANT(&context.vInstructions, expr.stackOffset);

                    if (p_hir->p_block_offsets[false_dest] != i + ketl_hir_decode_size(p_hir, i)) {
                        instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_JUMP);

                        placeholder = false_dest;
                        PUSH_CONSTANT(&context.vInstructions, placeholder);
                    }
                    
                    first_instr_in_block = true;
                    break;
                }
                case KETL_HIR_RETURN: {
                    instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_RETURN);
                    PUSH_CONSTANT(&context.vInstructions, stackReservedSize);
                    break;
                }
                case KETL_HIR_RETURN_VALUE: {
                    ketl_hir_return_value_t* p_hir_info = (ketl_hir_return_value_t*)p_instr;

                    arg_info arg0 = hir_get_arg_stack_offset(&context, p_hir_info->value_var);
                    
                    // TODO FIX
                    // get type size and use appropriate return bytecode
                    instructions_push_back_copy(&context.vInstructions, KETL_BYTECODE_64RETURN);
                    PUSH_CONSTANT(&context.vInstructions, stackReservedSize);
                    PUSH_CONSTANT(&context.vInstructions, arg0.stackOffset);
                    break;
                }
            }
        }
    }

    for (uint32_t i = 0u; i < context.vInstructions.size; 
            i += ketl_bytecode_decode_instruction_length(context.vInstructions.p_data[i])) {
        switch (*(ketl_bytecode_instr*)(context.vInstructions.p_data + i)) {
            case KETL_BYTECODE_JUMP:
            case KETL_BYTECODE_8JUMP_IF:
            case KETL_BYTECODE_16JUMP_IF:
            case KETL_BYTECODE_32JUMP_IF:
            case KETL_BYTECODE_64JUMP_IF: {
                ketl_hir_block_index_t block = *(ketl_bytecode_jump_offset*)(context.vInstructions.p_data + i + sizeof(ketl_bytecode_instr));
                instr_to_bytecode_offsets_bucket* p_bucket = instr_to_bytecode_offsets_get_or_null(&context.m_instr_to_bytecode_offsets, p_hir->p_block_offsets[block]); 
                *(ketl_bytecode_jump_offset*)(context.vInstructions.p_data + i + sizeof(ketl_bytecode_instr)) = (ketl_bytecode_jump_offset)p_bucket->value;
                break;
        }
        }
    }

    variables_deinit(&context.mVariables);
    instr_to_bytecode_offsets_deinit(&context.m_instr_to_bytecode_offsets);

    return (ketl_bytecode){
        .pInstructions = context.vInstructions.p_data,
        .instructionsCount = context.vInstructions.size,
    };
}
