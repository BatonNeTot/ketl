//🫖ketl
#include "hir.h"

#include "ketl_impl.h"

#include <stdio.h>

bool ketl_hir_is_terminator_tag(ketl_hir_tag_t tag) {
    switch(tag & KETL_HIR_TYPE_INSTR_MASK) {
        case KETL_HIR_JUMP_IF_EQUAL:
        case KETL_HIR_JUMP_IF_NOT_EQAUL:
        
        case KETL_HIR_JUMP_IF_LESS:
        case KETL_HIR_JUMP_IF_LESS_OR_EQAUL:
        
        case KETL_HIR_JUMP_IF_GREATER:
        case KETL_HIR_JUMP_IF_GREATER_OR_EQAUL:

        case KETL_HIR_RETURN_VALUE:
            return true;
    }

    switch (tag) {
        case KETL_HIR_JUMP:

        case KETL_HIR_JUMP_IF_TRUE:
        case KETL_HIR_JUMP_IF_FALSE:

        case KETL_HIR_RETURN:
            return true;
    }

    return false;
}

void ketl_hir_deinit(ketl_hir_t* p_hir) {
    ketl_free(p_hir->p_allocator, p_hir->p_symbols);
    ketl_free(p_hir->p_allocator, p_hir->p_vars);
    ketl_free(p_hir->p_allocator, p_hir->p_vars_infos);
    ketl_free(p_hir->p_allocator, p_hir->p_used_types);
    ketl_free(p_hir->p_allocator, p_hir->p_block_offsets);
    ketl_free(p_hir->p_allocator, p_hir->p_instrs);
}

#define HIR_CASE_CREATE(val) case ANN_CONCAT(HIR_CASE_PREFIX, val): 
#define HIR_CASE_CLAUSES(...) ANN_JOIN(, ANN_FOR_EACH(HIR_CASE_CREATE, __VA_ARGS__))

#define HIR_SUPPORTED_TYPES UNDEF, I8, I16, I32, I64

ketl_hir_instr_offset_t ketl_hir_get_instr_size(ketl_hir_tag_t tag, uint8_t* p_instr) {
    switch (tag & KETL_HIR_TYPE_INSTR_MASK) {
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
        case KETL_HIR_GREATER_OR_EQUAL:
            return sizeof(ketl_hir_binary_op_t);

        case KETL_HIR_ASSIGN:
            return sizeof(ketl_hir_assign_t);
    
        case KETL_HIR_JUMP_IF_EQUAL:
        case KETL_HIR_JUMP_IF_NOT_EQAUL:
    
        case KETL_HIR_JUMP_IF_LESS:
        case KETL_HIR_JUMP_IF_LESS_OR_EQAUL:
    
        case KETL_HIR_JUMP_IF_GREATER:
        case KETL_HIR_JUMP_IF_GREATER_OR_EQAUL:
            return sizeof(ketl_hir_jump_if_cmp_t);

        case KETL_HIR_RETURN_VALUE:
            return sizeof(ketl_hir_return_value_t);
    }

    ANN_SWITCH_STRICT (tag) {
        case KETL_HIR_NONE_STMT:
            return 0;

        case KETL_HIR_CALL_VOID: {
            ketl_hir_call_void_t* hir_info = (ketl_hir_call_void_t*)p_instr;
            return sizeof(ketl_hir_call_void_t) + hir_info->arguments_count * sizeof(*hir_info->a_arguments);
        }
        case KETL_HIR_CALL: {
            ketl_hir_call_t* hir_info = (ketl_hir_call_t*)p_instr;
            return sizeof(ketl_hir_call_t) + hir_info->arguments_count * sizeof(*hir_info->a_arguments);
        }
        case KETL_HIR_NEW: {
            ketl_hir_new_t* hir_info = (ketl_hir_new_t*)p_instr;
            return sizeof(ketl_hir_new_t) + hir_info->arguments_count * sizeof(*hir_info->a_arguments);
        }
        case KETL_HIR_CREATE_ARRAY:
            return sizeof(ketl_hir_create_array_t);
        case KETL_HIR_APPEND_VALUE:
            return sizeof(ketl_hir_append_value_t);

        case KETL_HIR_JUMP:
            return sizeof(ketl_hir_jump_t);
        case KETL_HIR_JUMP_IF_TRUE:
        case KETL_HIR_JUMP_IF_FALSE:
            return sizeof(ketl_hir_jump_if_t);
    
        case KETL_HIR_RETURN:
            return 0;
    }
}

ketl_hir_instr_offset_t ketl_hir_decode_size(ketl_hir_t* p_hir, ketl_hir_instr_offset_t instr_offset) {
    uint8_t* p_instr = p_hir->p_instrs + instr_offset;

    ketl_hir_header_t header = *(ketl_hir_header_t*)p_instr;
    p_instr += sizeof(ketl_hir_header_t);

    p_instr += ketl_hir_get_instr_size(header.tag, p_instr);

    return (ketl_hir_instr_offset_t)(p_instr - (p_hir->p_instrs + instr_offset));
}

static uint32_t ketl_hir_format_var(ketl_hir_t* p_hir, ketl_hir_var_id_t var_id, char* buffer, uint32_t buffer_size) {
    ketl_hir_var_t* p_var = p_hir->p_vars + var_id;
    if (p_var->uid == KETL_HIR_VAR_UID_LITERAL) {
        const char* p_literal = KETL_ATOMIC_STRING_GET_POINTER(p_hir->p_symbols, p_var->literal);
        return snprintf(buffer, buffer_size, "%s|%"PRIu16, p_literal != NULL ? p_literal : "null", 
            ketl_type_get_stack_size(p_hir->p_used_types[p_var->type]));
    } else if (p_var->info == KETL_HIR_VAR_INFO_TEMP) {
        uint32_t printed = 0;
        printed += snprintf(buffer + printed, buffer_size - printed, "~%"PRIu16, p_var->uid);
        if (p_var->type == KETL_HIR_USED_TYPE_UNKNOWN) {
            printed += snprintf(buffer + printed, buffer_size - printed, "|undef");
        } else {
            printed += snprintf(buffer + printed, buffer_size - printed, "|%"PRIu16,
                ketl_type_get_stack_size(p_hir->p_used_types[p_var->type]));
        }
        return printed;
    } 
    
    ketl_hir_var_info_t* p_var_info = p_hir->p_vars_infos + p_var->info;
    switch (p_var->uid) {
        case KETL_HIR_VAR_UID_PARAMETER:
        case KETL_HIR_VAR_UID_GLOBAL: {
            uint32_t printed = 0;
            printed += snprintf(buffer + printed, buffer_size - printed, "%s", 
                KETL_ATOMIC_STRING_GET_POINTER(p_hir->p_symbols, p_var_info->name));
            if (p_var->type == KETL_HIR_USED_TYPE_UNKNOWN) {
                printed += snprintf(buffer + printed, buffer_size - printed, "|undef");
            } else {
                printed += snprintf(buffer + printed, buffer_size - printed, "|%"PRIu16,
                    ketl_type_get_stack_size(p_hir->p_used_types[p_var->type]));
            }
            return printed;
        } 
        case KETL_HIR_VAR_UID_FIELD: {
            uint32_t printed = 0;

            printed += snprintf(buffer + printed, buffer_size - printed, "(");
            printed += ketl_hir_format_var(p_hir, p_var_info->parent_id, buffer + printed, buffer_size - printed);
            printed += snprintf(buffer + printed, buffer_size - printed, ").");

            printed += snprintf(buffer + printed, buffer_size - printed, "%s", 
                KETL_ATOMIC_STRING_GET_POINTER(p_hir->p_symbols, p_var_info->name));
            if (p_var->type == KETL_HIR_USED_TYPE_UNKNOWN) {
                printed += snprintf(buffer + printed, buffer_size - printed, "|undef");
            } else {
                printed += snprintf(buffer + printed, buffer_size - printed, "|%"PRIu16,
                    ketl_type_get_stack_size(p_hir->p_used_types[p_var->type]));
            }
            return printed;
        }
        case KETL_HIR_VAR_UID_INDEX: {
            uint32_t printed = 0;

            printed += snprintf(buffer + printed, buffer_size - printed, "(");
            printed += ketl_hir_format_var(p_hir, p_var_info->parent_id, buffer + printed, buffer_size - printed);
            printed += snprintf(buffer + printed, buffer_size - printed, ")[");

            printed += ketl_hir_format_var(p_hir, p_var_info->arg_id, buffer + printed, buffer_size - printed);
            printed += snprintf(buffer + printed, buffer_size - printed, "]");
            return printed;
        }
        default: {
            uint32_t printed = 0;

            printed += snprintf(buffer + printed, buffer_size - printed, "%s#%"PRIu16, 
                KETL_ATOMIC_STRING_GET_POINTER(p_hir->p_symbols, p_var_info->name), p_var->uid);
            if (p_var->type == KETL_HIR_USED_TYPE_UNKNOWN) {
                printed += snprintf(buffer + printed, buffer_size - printed, "|undef");
            } else {
                printed += snprintf(buffer + printed, buffer_size - printed, "|%"PRIu16,
                    ketl_type_get_stack_size(p_hir->p_used_types[p_var->type]));
            }
            return printed;
        }
    }
} 

static uint32_t ketl_hir_format_instr(ketl_state* p_state, ketl_hir_t* p_hir, ketl_hir_instr_offset_t instr_offset, char* buffer, uint32_t buffer_size) {
    (void)p_state;
    uint8_t* p_instr = p_hir->p_instrs + instr_offset;

    ketl_hir_header_t header = *(ketl_hir_header_t*)p_instr;
    p_instr += sizeof(ketl_hir_header_t);

    char var_buffer[4][256];
    
#define INIT_HIR_INFO(type) type* p_hir_info = (type*)p_instr
#define FORMAT_VAR(var_id, buffer) (ketl_hir_format_var(p_hir, var_id, buffer, ANN_ARRAY_SIZE(buffer)))
#define FORMAT_BLOCK(block_index, buffer) (snprintf(buffer, ANN_ARRAY_SIZE(buffer), "BB%"PRIu16, block_index))

    switch (header.tag & KETL_HIR_TYPE_INSTR_MASK) {
        case KETL_HIR_PLUS: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s + %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }
        case KETL_HIR_MINUS: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s - %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }
        case KETL_HIR_MULTY: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s * %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }
        case KETL_HIR_DIV: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s / %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }
        case KETL_HIR_MOD: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s %% %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }

        case KETL_HIR_EQUAL: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s == %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }
        case KETL_HIR_NOT_EQUAL: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s != %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }
        case KETL_HIR_LESS: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s < %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }
        case KETL_HIR_LESS_OR_EQUAL: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s <= %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }
        case KETL_HIR_GREATER: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s > %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }
        case KETL_HIR_GREATER_OR_EQUAL: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "%s = %s >= %s;",
                var_buffer[0], var_buffer[1], var_buffer[2]);
        }

        case KETL_HIR_ASSIGN: {
            INIT_HIR_INFO(ketl_hir_assign_t);
            FORMAT_VAR(p_hir_info->dest_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->source_var, var_buffer[1]);
            return snprintf(buffer, buffer_size, "%s = %s;",
                var_buffer[0], var_buffer[1]);
        }
    
        case KETL_HIR_JUMP_IF_EQUAL: {
            INIT_HIR_INFO(ketl_hir_jump_if_cmp_t);
            FORMAT_BLOCK(p_hir_info->true_block, var_buffer[0]);
            FORMAT_BLOCK(p_hir_info->false_block, var_buffer[1]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[2]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[3]);
            return snprintf(buffer, buffer_size, "if (%s == %s) goto %s; else goto %s;",
                var_buffer[2], var_buffer[3], var_buffer[0], var_buffer[1]);
        }
        case KETL_HIR_JUMP_IF_NOT_EQAUL: {
            INIT_HIR_INFO(ketl_hir_jump_if_cmp_t);
            FORMAT_BLOCK(p_hir_info->true_block, var_buffer[0]);
            FORMAT_BLOCK(p_hir_info->false_block, var_buffer[1]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[2]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[3]);
            return snprintf(buffer, buffer_size, "if (%s != %s) goto %s; else goto %s;",
                var_buffer[2], var_buffer[3], var_buffer[0], var_buffer[1]);
        }
    
        case KETL_HIR_JUMP_IF_LESS: {
            INIT_HIR_INFO(ketl_hir_jump_if_cmp_t);
            FORMAT_BLOCK(p_hir_info->true_block, var_buffer[0]);
            FORMAT_BLOCK(p_hir_info->false_block, var_buffer[1]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[2]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[3]);
            return snprintf(buffer, buffer_size, "if (%s < %s) goto %s; else goto %s;",
                var_buffer[2], var_buffer[3], var_buffer[0], var_buffer[1]);
        }
        case KETL_HIR_JUMP_IF_LESS_OR_EQAUL: {
            INIT_HIR_INFO(ketl_hir_jump_if_cmp_t);
            FORMAT_BLOCK(p_hir_info->true_block, var_buffer[0]);
            FORMAT_BLOCK(p_hir_info->false_block, var_buffer[1]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[2]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[3]);
            return snprintf(buffer, buffer_size, "if (%s <= %s) goto %s; else goto %s;",
                var_buffer[2], var_buffer[3], var_buffer[0], var_buffer[1]);
        }
    
        case KETL_HIR_JUMP_IF_GREATER: {
            INIT_HIR_INFO(ketl_hir_jump_if_cmp_t);
            FORMAT_BLOCK(p_hir_info->true_block, var_buffer[0]);
            FORMAT_BLOCK(p_hir_info->false_block, var_buffer[1]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[2]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[3]);
            return snprintf(buffer, buffer_size, "if (%s > %s) goto %s; else goto %s;",
                var_buffer[2], var_buffer[3], var_buffer[0], var_buffer[1]);
        }
        case KETL_HIR_JUMP_IF_GREATER_OR_EQAUL: {
            INIT_HIR_INFO(ketl_hir_jump_if_cmp_t);
            FORMAT_BLOCK(p_hir_info->true_block, var_buffer[0]);
            FORMAT_BLOCK(p_hir_info->false_block, var_buffer[1]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[2]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[3]);
            return snprintf(buffer, buffer_size, "if (%s >= %s) goto %s; else goto %s;",
                var_buffer[2], var_buffer[3], var_buffer[0], var_buffer[1]);
        }

        case KETL_HIR_RETURN_VALUE: {
            INIT_HIR_INFO(ketl_hir_return_value_t);
            FORMAT_VAR(p_hir_info->value_var, var_buffer[0]);
            return snprintf(buffer, buffer_size, "return %s;",
                var_buffer[0]);
        }
    }

    ANN_SWITCH_STRICT (header.tag) {
        case KETL_HIR_NONE_STMT:
            return snprintf(buffer, buffer_size, "none;");        

        case KETL_HIR_CALL_VOID: {
            INIT_HIR_INFO(ketl_hir_call_void_t);
            FORMAT_VAR(p_hir_info->callee, var_buffer[0]);
            uint32_t count = snprintf(buffer, buffer_size, "%s(",
                var_buffer[0]);
            for (uint32_t i = 0u; i < p_hir_info->arguments_count; ++i) {
                if (i != 0) {
                    count += snprintf(buffer + count, buffer_size - count, ", ");
                }
                count += ketl_hir_format_var(p_hir, p_hir_info->a_arguments[i], buffer + count, buffer_size - count);
            }
            count += snprintf(buffer + count, buffer_size - count, ");");
            return count;
        }
        case KETL_HIR_CALL: {
            INIT_HIR_INFO(ketl_hir_call_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->callee, var_buffer[1]);
            uint32_t count = snprintf(buffer, buffer_size, "%s = %s(",
                var_buffer[0], var_buffer[1]);
            for (uint32_t i = 0u; i < p_hir_info->arguments_count; ++i) {
                if (i != 0) {
                    count += snprintf(buffer + count, buffer_size - count, ", ");
                }
                count += ketl_hir_format_var(p_hir, p_hir_info->a_arguments[i], buffer + count, buffer_size - count);
            }
            count += snprintf(buffer + count, buffer_size - count, ");");
            return count;
        }

        case KETL_HIR_NEW: {
            INIT_HIR_INFO(ketl_hir_new_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->type_var, var_buffer[1]);
            uint32_t count = snprintf(buffer, buffer_size, "%s = %s(", var_buffer[0], var_buffer[1]);
            for (uint32_t i = 0u; i < p_hir_info->arguments_count; ++i) {
                if (i != 0) {
                    count += snprintf(buffer + count, buffer_size - count, ", ");
                }
                count += ketl_hir_format_var(p_hir, p_hir_info->a_arguments[i], buffer + count, buffer_size - count);
            }
            count += snprintf(buffer + count, buffer_size - count, ");");
            return count;
        }
        case KETL_HIR_CREATE_ARRAY: {
            INIT_HIR_INFO(ketl_hir_create_array_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->type_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->count_var_id, var_buffer[2]);
            uint32_t count = snprintf(buffer, buffer_size, "%s = %s(%s)", var_buffer[0], var_buffer[1], var_buffer[2]);
            return count;
        }

        case KETL_HIR_JUMP: {
            INIT_HIR_INFO(ketl_hir_jump_t);
            FORMAT_BLOCK(p_hir_info->block_index, var_buffer[0]);
            return snprintf(buffer, buffer_size, "goto %s;",
                var_buffer[0]);
        }

        case KETL_HIR_JUMP_IF_TRUE: {
            INIT_HIR_INFO(ketl_hir_jump_if_t);
            FORMAT_BLOCK(p_hir_info->true_block, var_buffer[0]);
            FORMAT_BLOCK(p_hir_info->false_block, var_buffer[1]);
            FORMAT_VAR(p_hir_info->expr_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "if (%s) goto %s; else goto %s;",
                var_buffer[2], var_buffer[0], var_buffer[1]);
        }

        case KETL_HIR_JUMP_IF_FALSE: {
            INIT_HIR_INFO(ketl_hir_jump_if_t);
            FORMAT_BLOCK(p_hir_info->true_block, var_buffer[0]);
            FORMAT_BLOCK(p_hir_info->false_block, var_buffer[1]);
            FORMAT_VAR(p_hir_info->expr_var, var_buffer[2]);
            return snprintf(buffer, buffer_size, "if (!%s) goto %s; else goto %s;",
                var_buffer[2], var_buffer[0], var_buffer[1]);
        }
    
        case KETL_HIR_RETURN:
            return snprintf(buffer, buffer_size, "return;");
    }
}

uint32_t ketl_hir_format(ketl_state* p_state, ketl_hir_t* p_hir, char* p_buffer, uint32_t buffer_size) {
    uint32_t printed_count = 0;

    for (uint32_t block = 0u; block < p_hir->blocks_count; ++block) {
        uint32_t tab_size = snprintf(p_buffer + printed_count, buffer_size - printed_count, "BB%"PRIu16": ", block);
        printed_count += tab_size;

        uint32_t i = p_hir->p_block_offsets[block];
        for (; i < p_hir->instrs_count; i += ketl_hir_decode_size(p_hir, i)) {
            if (i != p_hir->p_block_offsets[block]) {
                printed_count += snprintf(p_buffer + printed_count, buffer_size - printed_count, "%*s", tab_size, "");
            }

            printed_count += ketl_hir_format_instr(p_state, p_hir, i, p_buffer + printed_count, buffer_size - printed_count);
            printed_count += snprintf(p_buffer + printed_count, buffer_size - printed_count, "\n");

            if (ketl_hir_is_terminator_tag((*(ketl_hir_header_t*)&p_hir->p_instrs[i]).tag)) {
                break;
            }
        }
    }

    return printed_count;
}
