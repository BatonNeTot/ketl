//🫖ketl
#include "hir.h"

#include <stdio.h>

bool ketl_hir_is_terminator_tag(ketl_hir_tag_t tag) {
    return tag >= KETL_HIR_RETURN;
}

void ketl_hir_deinit(ketl_hir_t* p_hir) {
    ketl_free(p_hir->p_allocator, p_hir->p_symbols);
    ketl_free(p_hir->p_allocator, p_hir->p_used_types);
    ketl_free(p_hir->p_allocator, p_hir->p_block_offsets);
    ketl_free(p_hir->p_allocator, p_hir->p_vars);
    ketl_free(p_hir->p_allocator, p_hir->p_instrs);
}

ketl_hir_instr_offset_t ketl_hir_get_pos_size(ketl_hir_pos_tag_t pos_tag) {
    KETL_SWITCH_STRICT (pos_tag) {
        case KETL_HIR_POS_NONE:
            return 0;
        case KETL_HIR_POS_COL:
            return sizeof(ketl_hir_pos_col_t);
        case KETL_HIR_POS_LINE:
            return sizeof(ketl_hir_pos_line_t);
        case KETL_HIR_POS_FILE:
            return sizeof(ketl_hir_pos_file_t);
    }
}

ketl_hir_instr_offset_t ketl_hir_get_instr_size(ketl_hir_tag_t tag, uint8_t* p_instr) {
    KETL_SWITCH_STRICT (tag) {
        case KETL_HIR_NONE_STMT:
            return 0;

        case KETL_HIR_PLUS_UNDEF:
        case KETL_HIR_MINUS_UNDEF:
        case KETL_HIR_MULTY_UNDEF:
        case KETL_HIR_DIV_UNDEF:
        case KETL_HIR_PLUS_I64:
        case KETL_HIR_MINUS_I64:
        case KETL_HIR_MULTY_I64:
        case KETL_HIR_DIV_I64:
            return sizeof(ketl_hir_binary_op_t);

        case KETL_HIR_CALL_VOID: {
            ketl_hir_call_void_t* hir_info = (ketl_hir_call_void_t*)p_instr;
            return sizeof(ketl_hir_call_void_t) + hir_info->arguments_count * sizeof(*hir_info->arguments);
        }
        case KETL_HIR_CALL: {
            ketl_hir_call_t* hir_info = (ketl_hir_call_t*)p_instr;
            return sizeof(ketl_hir_call_t) + hir_info->arguments_count * sizeof(*hir_info->arguments);
        }
    
        case KETL_HIR_RETURN:
            return 0;
        case KETL_HIR_RETURN_VALUE:
            return sizeof(ketl_hir_return_value_t);
    }
}

ketl_hir_instr_offset_t ketl_hir_decode_size(ketl_hir_t* p_hir, ketl_hir_instr_offset_t instr_offset) {
    uint8_t* p_instr = p_hir->p_instrs + instr_offset;

    ketl_hir_header_t header = *(ketl_hir_header_t*)p_instr;
    p_instr += sizeof(ketl_hir_header_t);

    p_instr += ketl_hir_get_pos_size(header.start_pos_tag);
    p_instr += ketl_hir_get_pos_size(header.end_pos_tag);

    p_instr += ketl_hir_get_instr_size(header.tag, p_instr);

    return p_instr - (p_hir->p_instrs + instr_offset);
}

static uint32_t ketl_hir_format_var(ketl_hir_t* p_hir, ketl_hir_var_id_t var_id, char* buffer, uint32_t bufferSize) {
    if (p_hir->p_vars[var_id].name == KETL_ATOMIC_STRING_EMPTY) {
        return snprintf(buffer, bufferSize, "~%"PRIu16, 
            p_hir->p_vars[var_id].uid);
    } else if (p_hir->p_vars[var_id].uid == KETL_HIR_VAR_UID_LITERAL) {
        return snprintf(buffer, bufferSize, "%s", 
            KETL_ATOMIC_STRING_GET_POINTER(p_hir->p_symbols, p_hir->p_vars[var_id].name));
    } else {
        return snprintf(buffer, bufferSize, "%s#%"PRIu16, 
            KETL_ATOMIC_STRING_GET_POINTER(p_hir->p_symbols, p_hir->p_vars[var_id].name), p_hir->p_vars[var_id].uid);
    }
} 

static uint32_t ketl_hir_format_instr(ketl_hir_t* p_hir, ketl_hir_instr_offset_t instr_offset, char* buffer, uint32_t bufferSize) {
    uint8_t* p_instr = p_hir->p_instrs + instr_offset;

    ketl_hir_header_t header = *(ketl_hir_header_t*)p_instr;
    p_instr += sizeof(ketl_hir_header_t);

    p_instr += ketl_hir_get_pos_size(header.start_pos_tag);
    p_instr += ketl_hir_get_pos_size(header.end_pos_tag);

    char var_buffer[3][256];
    
#define INIT_HIR_INFO(type) type* p_hir_info = (type*)p_instr
#define FORMAT_VAR(var_id, buffer) ketl_hir_format_var(p_hir, var_id, buffer, KETL_ARRAY_SIZE(buffer))

    KETL_SWITCH_STRICT (header.tag) {
        case KETL_HIR_NONE_STMT:
            return snprintf(buffer, bufferSize, "none");

        case KETL_HIR_PLUS_UNDEF:
        case KETL_HIR_PLUS_I64:{
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, bufferSize, "%s = %s + %s",
                var_buffer[0], var_buffer[1], var_buffer[2]);
            }
        case KETL_HIR_MINUS_UNDEF:
        case KETL_HIR_MINUS_I64: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, bufferSize, "%s = %s - %s",
                var_buffer[0], var_buffer[1], var_buffer[2]);
            }
        case KETL_HIR_MULTY_UNDEF:
        case KETL_HIR_MULTY_I64: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, bufferSize, "%s = %s * %s",
                var_buffer[0], var_buffer[1], var_buffer[2]);
            }
        case KETL_HIR_DIV_UNDEF:
        case KETL_HIR_DIV_I64: {
            INIT_HIR_INFO(ketl_hir_binary_op_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->lhs_var, var_buffer[1]);
            FORMAT_VAR(p_hir_info->rhs_var, var_buffer[2]);
            return snprintf(buffer, bufferSize, "%s = %s / %s",
                var_buffer[0], var_buffer[1], var_buffer[2]);
            }

        case KETL_HIR_CALL_VOID: {
            INIT_HIR_INFO(ketl_hir_call_void_t);
            FORMAT_VAR(p_hir_info->callee, var_buffer[0]);
            uint32_t count = snprintf(buffer, bufferSize, "%s(",
                var_buffer[0]);
            for (uint32_t i = 0u; i < p_hir_info->arguments_count; ++i) {
                if (i != 0) {
                    count += snprintf(buffer + count, bufferSize - count, ", ");
                }
                count += ketl_hir_format_var(p_hir, p_hir_info->arguments[i], buffer + count, bufferSize - count);
            }
            count += snprintf(buffer + count, bufferSize - count, ")");
            return count;
        }
        case KETL_HIR_CALL: {
            INIT_HIR_INFO(ketl_hir_call_t);
            FORMAT_VAR(p_hir_info->output_var, var_buffer[0]);
            FORMAT_VAR(p_hir_info->callee, var_buffer[1]);
            uint32_t count = snprintf(buffer, bufferSize, "%s = %s(",
                var_buffer[0], var_buffer[1]);
            for (uint32_t i = 0u; i < p_hir_info->arguments_count; ++i) {
                if (i != 0) {
                    count += snprintf(buffer + count, bufferSize - count, ", ");
                }
                count += ketl_hir_format_var(p_hir, p_hir_info->arguments[i], buffer + count, bufferSize - count);
            }
            count += snprintf(buffer + count, bufferSize - count, ")");
            return count;
        }
    
        case KETL_HIR_RETURN:
            return snprintf(buffer, bufferSize, "return");
        case KETL_HIR_RETURN_VALUE: {
            INIT_HIR_INFO(ketl_hir_return_value_t);
            FORMAT_VAR(p_hir_info->value_var, var_buffer[0]);
            return snprintf(buffer, bufferSize, "return %s",
                var_buffer[0]);
            }
    }
}

uint32_t ketl_hir_format(ketl_hir_t* p_hir, char* p_buffer, uint32_t buffer_size) {
    uint32_t printed_count = 0;

    for (uint32_t block = 0u; block < p_hir->blocks_count; ++block) {
        uint32_t tab_size = snprintf(p_buffer + printed_count, buffer_size - printed_count, "BB%"PRIu16": ", block);
        printed_count += tab_size;

        uint32_t i = p_hir->p_block_offsets[block];
        for (; i < p_hir->instrs_count && !ketl_hir_is_terminator_tag(p_hir->p_instrs[i]); i += ketl_hir_decode_size(p_hir, i)) {
            if (i != p_hir->p_block_offsets[block]) {
                printed_count += snprintf(p_buffer + printed_count, buffer_size - printed_count, "%*s", tab_size, "");
            }
            printed_count += ketl_hir_format_instr(p_hir, i, p_buffer + printed_count, buffer_size - printed_count);
            printed_count += snprintf(p_buffer + printed_count, buffer_size - printed_count, "\n");
        }

        // print terminator instr
        if (i != p_hir->p_block_offsets[block]) {
            printed_count += snprintf(p_buffer + printed_count, buffer_size - printed_count, "%*s", tab_size, "");
        }
        printed_count += ketl_hir_format_instr(p_hir, i, p_buffer + printed_count, buffer_size - printed_count);
        printed_count += snprintf(p_buffer + printed_count, buffer_size - printed_count, "\n");
    }

    return printed_count;
}
