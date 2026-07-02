//🫖ketl
#include "compiler/parser.h"

#include "ketl_impl.h"

#include "error_stream.h"

#include "compiler/lexer.h"

#include "namespace.h"

#include "executable_memory.h"
#include "dynamic_library.h"
#include "compiler/assembler.h"

#include "containers/vector.h"
#include "containers/hash_map.h"
#include "memory_impl.h"
#include "str.h"

#include <stdio.h>
#include <stdlib.h>

typedef uint16_t lalr_state;

KETL_VECTOR_DECLARATION(_ketl_parse_argument_stack_t, ketl_hir_var_id_t)
KETL_VECTOR_DEFINITION(_ketl_parse_argument_stack_t, ketl_hir_var_id_t)

KETL_HASH_MAP_DECLARATION(_ketl_parse_undefined_vars_infos_t, ketl_hir_var_info_index_t, bool)
KETL_HASH_MAP_DEFINITION(_ketl_parse_undefined_vars_infos_t, ketl_hir_var_info_index_t, bool, ANN_HASH, ANN_EQUAL)

ANN_DEFINE(ketl_parser_context) {
    ketl_state* p_state;
    ketl_lexer_t* p_lexer;
    ketl_namespace* p_namespace;
    ketl_token_iterator_t end_pos;
    ketl_hir_symbol_offset_t s_filename;

    ketl_hir_block_index_t reserved_break;
    ketl_hir_block_index_t reserved_continue;
    bool is_global_scope;
    bool export;
    bool cexport;
    bool next_symbol_entry;

    ketl_hir_builder_t hir_builder;

    _ketl_parse_argument_stack_t v_argument_stack;
    _ketl_parse_undefined_vars_infos_t m_undef_vars_infos;
};

typedef uint8_t ketl_parse_return_info;
enum {
    KETL_RETURN_EMPTY        = 0x0,
    KETL_RETURN_NONE         = 0x1,
    KETL_RETURN_VALUE        = 0x2,
    KETL_RETURN_UNDEF        = KETL_RETURN_NONE  | KETL_RETURN_VALUE,
    KETL_RETURN_ALWAYS       = 0x4,

    KETL_RETURN_ALWAYS_NONE  = KETL_RETURN_NONE  | KETL_RETURN_ALWAYS,
    KETL_RETURN_ALWAYS_VALUE = KETL_RETURN_VALUE | KETL_RETURN_ALWAYS,
    KETL_RETURN_ALWAYS_UNDEF = KETL_RETURN_UNDEF | KETL_RETURN_ALWAYS,
};

ANN_DEFINE(ketl_statement_info) {
    ketl_parse_return_info return_info;
};

typedef uint8_t ketl_parser_bracket_t;
enum {
    KETL_PARSER_BRACKET_PARENTHESIS,
    KETL_PARSER_BRACKET_CURLY,
    KETL_PARSER_BRACKET_SQUARE,
};

#define EOF_STR "EOF"
#define TOKEN_LENGTH(token) ((int)((token).length > 0 ? (token).length : sizeof(EOF_STR) - 1))
#define TOKEN_STRING(token) ((token).length > 0 ? (p_context)->p_lexer->p_source + (token).offset : EOF_STR)

#define TOKEN(index) ((p_context)->p_lexer->tokens.p_data[(index)])
#define CURRENT_TOKEN(offset) (get_current_token(p_context, (offset)))

#define GET_SYMBOL_SOURCE(symbol_offset) (ketl_atomic_strings_get_pointer(&p_context->hir_builder.symbols, (symbol_offset)))


#define errorf(__offset, __length, ...) \
do {\
    ketl_error_info error_info = {\
        .p_lexer = p_context->p_lexer,\
        .s_filename = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, GET_SYMBOL_SOURCE(p_context->s_filename), KETL_NULL_TERMINATED_LENGTH_32),\
        .offset = (__offset),\
        .length = (__length),\
    };\
    ketl_error_report(p_context->p_state, &error_info, __VA_ARGS__);\
} while (0)

static ketl_token_t get_eof_token(ketl_parser_context* p_context) {
    return (ketl_token_t){
            .type = KETL_TOKEN_TYPE_EOF, 
            .length = (ketl_token_length_t)0, 
            .offset = TOKEN(p_context->end_pos - 1).offset,
    };
}

static ketl_token_t get_current_token(ketl_parser_context* p_context, ketl_token_iterator_t offset) {
    ketl_token_iterator_t index = (p_context)->p_lexer->token_iterator - (offset);
    if (index >= p_context->end_pos) {
        return get_eof_token(p_context);
    }
    return TOKEN(index);
}

#define GET_VAR(var_id) (p_context->hir_builder.vars.p_data[(var_id)])
#define GET_TYPE(used_type_index) (p_context->hir_builder.used_types.p_data[(used_type_index)])

static ketl_hir_var_info_t* get_var_info(ketl_parser_context* p_context, ketl_hir_var_info_index_t var_info_index) {
    return var_info_index != KETL_HIR_VAR_INFO_TEMP ? &p_context->hir_builder.vars_infos.p_data[var_info_index] : NULL;
} 

static ketl_hir_expr_info_t token_extract_info(ketl_token_t token) {
    return (ketl_hir_expr_info_t){.length = token.length, .source_offset = token.offset};
}

static ketl_token_offset_t expr_info_get_end(ketl_hir_expr_info_t expr_info) {
    return expr_info.source_offset + expr_info.length;
}

static ketl_hir_expr_info_t expr_info_merge(ketl_hir_expr_info_t expr_info_lhs, ketl_hir_expr_info_t expr_info_rhs) {
    return (ketl_hir_expr_info_t){
        .source_offset = expr_info_lhs.source_offset,
        .length = expr_info_get_end(expr_info_rhs) - expr_info_lhs.source_offset,
    };
}

static ketl_hir_symbol_offset_t push_symbol_string(ketl_parser_context* p_context, const char* p_str, uint32_t length) {
    return (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_context->hir_builder.symbols, p_str, length);
}

static ketl_hir_symbol_offset_t push_symbol(ketl_parser_context* p_context, ketl_token_t literal) {
    return push_symbol_string(p_context, TOKEN_STRING(literal), TOKEN_LENGTH(literal));
}

static ketl_hir_var_id_t push_literal_symbol_of_type(ketl_parser_context* p_context, ketl_hir_symbol_offset_t literal, ketl_hir_expr_info_t expr_info, ketl_hir_used_type_index_t type) {
    return ketl_hir_builder_get_literal(&p_context->hir_builder, literal, expr_info, type);
}

static ketl_hir_var_id_t push_literal_number_symbol(ketl_parser_context* p_context, ketl_hir_symbol_offset_t literal, ketl_hir_expr_info_t expr_info) {
    return push_literal_symbol_of_type(p_context, literal, expr_info, KETL_HIR_USED_TYPE_LITERAL);
}

static ketl_hir_var_id_t push_literal_number(ketl_parser_context* p_context, ketl_token_t literal_token) {
    return push_literal_number_symbol(p_context, push_symbol(p_context, literal_token), token_extract_info(literal_token));
}


static ketl_hir_var_id_t push_temp_var_type(ketl_parser_context* p_context, ketl_hir_expr_info_t expr_info, ketl_hir_used_type_index_t type) {
    return ketl_hir_builder_create_temp_var(&p_context->hir_builder, expr_info, type);
}
static ketl_hir_var_id_t push_temp_var(ketl_parser_context* p_context, ketl_hir_expr_info_t expr_info) {
    return push_temp_var_type(p_context, expr_info, KETL_HIR_USED_TYPE_UNKNOWN);
}

static ketl_hir_var_id_t push_hir_unary_op(ketl_parser_context* p_context, ketl_hir_tag_t hir_tag, ketl_hir_var_id_t rhs, ketl_hir_expr_info_t expr_info) {
    ketl_hir_header_t header = {
        .tag = hir_tag,
        .file_symbol = p_context->s_filename,
    };
    ketl_hir_var_id_t output_var = push_temp_var(p_context, expr_info); 
    ketl_hir_unary_op_t instr = {
        .output_var = output_var,
        .arg_var = rhs,
    };
    ketl_hir_builder_insert_unary_op(&p_context->hir_builder, header, &instr, expr_info);
    return output_var;
}

static ketl_hir_var_id_t push_hir_binary_op(ketl_parser_context* p_context, ketl_hir_tag_t hir_tag, ketl_hir_var_id_t lhs, ketl_hir_var_id_t rhs, ketl_hir_expr_info_t expr_info) {
    ketl_hir_header_t header = {
        .tag = hir_tag,
        .file_symbol = p_context->s_filename,
    };
    ketl_hir_var_id_t output_var = push_temp_var(p_context, expr_info); 
    ketl_hir_binary_op_t instr = {
        .output_var = output_var,
        .lhs_var = lhs,
        .rhs_var = rhs,
    };
    ketl_hir_builder_insert_binary_op(&p_context->hir_builder, header, &instr, expr_info);
    return output_var;
}

static void push_hir_argument(ketl_parser_context* p_context, ketl_hir_var_id_t var_id) {
    _ketl_parse_argument_stack_t_push_back_copy(&p_context->v_argument_stack, var_id);
}

static ketl_hir_var_id_t push_hir_call(ketl_parser_context* p_context, ketl_hir_var_id_t callee_id, uint16_t arguments_count, ketl_hir_expr_info_t expr_info);

// returns rhs after casting if happened
// else returns temp
static ketl_hir_var_id_t trying_to_cast_rhs_to_lhs(ketl_parser_context* p_context, ketl_hir_used_type_index_t lhs_type, ketl_hir_expr_info_t lhs_info, ketl_hir_var_id_t rhs_var, bool explicit) {
    ketl_hir_var_t* p_rhs_var = &GET_VAR(rhs_var);

    ketl_hir_expr_info_t expr_info = expr_info_merge(lhs_info, p_rhs_var->expr_info);

    if (p_rhs_var->type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return push_temp_var(p_context, expr_info);
    }

    if (p_rhs_var->type == KETL_HIR_USED_TYPE_LITERAL && GET_TYPE(lhs_type)->kind == KETL_TYPE_PRIMITIVE) {
        p_rhs_var->type = lhs_type;
        return rhs_var;
    }

    if (p_rhs_var->type == KETL_HIR_USED_TYPE_LITERAL && GET_TYPE(lhs_type)->kind == KETL_TYPE_ENUM && explicit) {
        p_rhs_var->type = lhs_type;
        return rhs_var;
    }

    if (lhs_type == p_rhs_var->type) {
        return rhs_var;
    }
    if (p_rhs_var->type >= KETL_HIR_USED_TYPE_LAST) {
        // TODO more informative message
        errorf(expr_info.source_offset, expr_info.length, "Incompatible for cast types.");
        return push_temp_var(p_context, expr_info);
    }

    ketl_type* p_lhs_type = GET_TYPE(lhs_type);
    ketl_type* p_rhs_type = GET_TYPE(p_rhs_var->type);

    if (p_rhs_type->kind == KETL_TYPE_ENUM) {
        p_rhs_type = (ketl_type*)((ketl_type_enum*)p_rhs_type)->p_parent_primitive;
    }

    if ((p_lhs_type->kind == KETL_TYPE_ARRAY && p_rhs_type->kind == KETL_TYPE_ARRAY && 
        ((ketl_type_array*)p_lhs_type)->p_value_type == ((ketl_type_array*)p_rhs_type)->p_value_type)) {
        return rhs_var;
    }

    if ((ketl_type_is_pointer_type(p_lhs_type) && ketl_type_is_raw_type(p_rhs_type)) ||
        (ketl_type_is_pointer_type(p_rhs_type) && ketl_type_is_raw_type(p_lhs_type))) {
            ketl_hir_var_id_t casted_var = ketl_hir_builder_cast_primitive(&p_context->hir_builder, rhs_var, lhs_type);
            return casted_var;
    }

    if ((ketl_type_is_u64_type(p_lhs_type) && ketl_type_is_raw_type(p_rhs_type)) ||
        (ketl_type_is_u64_type(p_rhs_type) && ketl_type_is_raw_type(p_lhs_type))) {
            ketl_hir_var_id_t casted_var = ketl_hir_builder_cast_primitive(&p_context->hir_builder, rhs_var, lhs_type);
            return casted_var;
    }
        
    // trying to primitive cast
    ketl_type_primitive* p_primitive_lhs_type = NULL;
    if (p_lhs_type->kind == KETL_TYPE_PRIMITIVE) {
        p_primitive_lhs_type = (ketl_type_primitive*)p_lhs_type;
    } else if (p_lhs_type->kind == KETL_TYPE_ENUM) {
        p_primitive_lhs_type = ((ketl_type_enum*)p_lhs_type)->p_parent_primitive;
    }

    if (p_primitive_lhs_type != NULL && p_rhs_type->kind == KETL_TYPE_PRIMITIVE && 
        p_primitive_lhs_type->is_integer && ((ketl_type_primitive*)p_rhs_type)->is_integer) {
            
        if (explicit || (p_primitive_lhs_type->size >= p_rhs_type->size && 
            p_primitive_lhs_type->is_signed == ((ketl_type_primitive*)p_rhs_type)->is_signed)) {
            ketl_hir_var_id_t casted_var = ketl_hir_builder_cast_primitive(&p_context->hir_builder, rhs_var, lhs_type);
            return casted_var;
        }
    }

    if (p_rhs_type->kind == KETL_TYPE_ARRAY && p_lhs_type->kind == KETL_TYPE_CARRAY && 
        ((ketl_type_array*)p_lhs_type)->p_value_type == ((ketl_type_array*)p_rhs_type)->p_value_type) {

        ketl_hir_symbol_offset_t converter_symbol = push_symbol_string(p_context, "str2cstr", 8);
        ketl_hir_var_id_t converter_id = ketl_hir_builder_get_var(&p_context->hir_builder, &p_context->p_state->secret_namespace, converter_symbol, converter_symbol, expr_info, true);
        ANN_ASSERT(GET_VAR(converter_id).type != KETL_HIR_USED_TYPE_UNKNOWN);

        push_hir_argument(p_context, rhs_var);
        ketl_hir_var_id_t convertion_call_id = push_hir_call(p_context, converter_id, 1, expr_info);

        return convertion_call_id;
    }

    if ((p_rhs_var->type == KETL_HIR_USED_TYPE_LITERAL || 
        (p_rhs_type->kind == KETL_TYPE_PRIMITIVE && ((ketl_type_primitive*)p_rhs_type)->is_numeric)) &&
        p_lhs_type->kind == KETL_TYPE_ARRAY && 
        ketl_type_is_char_type(((ketl_type_array*)p_lhs_type)->p_value_type)) {

        ketl_hir_symbol_offset_t converter_symbol;
        if (((ketl_type_primitive*)p_rhs_type)->is_signed) {
            converter_symbol = push_symbol_string(p_context, "int2str", 7);
        } else {
            converter_symbol = push_symbol_string(p_context, "uint2str", 8);
        }
        ketl_hir_var_id_t converter_id = ketl_hir_builder_get_var(&p_context->hir_builder, &p_context->p_state->secret_namespace, converter_symbol, converter_symbol, expr_info, true);
        ANN_ASSERT(GET_VAR(converter_id).type != KETL_HIR_USED_TYPE_UNKNOWN);

        push_hir_argument(p_context, rhs_var);
        ketl_hir_var_id_t convertion_call_id = push_hir_call(p_context, converter_id, 1, expr_info);

        return convertion_call_id;
    }

    if ((p_lhs_type->kind == KETL_TYPE_PRIMITIVE && ((ketl_type_primitive*)p_lhs_type)->is_numeric && p_lhs_type->size == 8) &&
        p_rhs_type->kind == KETL_TYPE_ARRAY && 
        ketl_type_is_char_type(((ketl_type_array*)p_rhs_type)->p_value_type)) {

        ketl_hir_symbol_offset_t converter_symbol = push_symbol_string(p_context, "str2uint", 8);
        ketl_hir_var_id_t converter_id = ketl_hir_builder_get_var(&p_context->hir_builder, &p_context->p_state->secret_namespace, converter_symbol, converter_symbol, expr_info, true);
        ANN_ASSERT(GET_VAR(converter_id).type != KETL_HIR_USED_TYPE_UNKNOWN);

        push_hir_argument(p_context, rhs_var);
        ketl_hir_var_id_t convertion_call_id = push_hir_call(p_context, converter_id, 1, expr_info);

        return convertion_call_id;
    }

    if (p_rhs_type->kind == KETL_TYPE_CLASS && p_lhs_type->kind == KETL_TYPE_CLASS) {
        bool related = false;
        int16_t signed_extended_offset;

        uint16_t extended_offset = ketl_type_find_extended_class_offset(p_rhs_type, p_lhs_type);
        if (extended_offset != (uint16_t)-1) {
            related = true;
            signed_extended_offset = extended_offset;
        } else if (explicit) {
            extended_offset = ketl_type_find_extended_class_offset(p_lhs_type, p_rhs_type);
            if (extended_offset != (uint16_t)-1) {
                related = true;
                signed_extended_offset = -extended_offset;
            }
        }

        if (related) {
            ketl_hir_var_id_t raw_var = trying_to_cast_rhs_to_lhs(p_context, 
                ketl_hir_builder_get_used_type_index(&p_context->hir_builder, ketl_state_get_raw_type(p_context->p_state)), 
                expr_info, rhs_var, false);

            if (signed_extended_offset != 0) {
                // TODO
                ANN_ASSERT(signed_extended_offset > 0);

                char a_offset_buffer[16];
                uint32_t offset_length = snprintf(a_offset_buffer, ANN_ARRAY_SIZE(a_offset_buffer), "%"PRId16, signed_extended_offset);
                ketl_hir_symbol_offset_t offset_symbol = push_symbol_string(p_context, a_offset_buffer, offset_length);

                ketl_hir_used_type_index_t size_type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, ketl_state_get_u64(p_context->p_state));
                ketl_hir_var_id_t offset_var = ketl_hir_builder_get_literal(&p_context->hir_builder, offset_symbol, expr_info, size_type);
                raw_var = push_hir_binary_op(p_context, KETL_HIR_PLUS, raw_var, offset_var, expr_info);
            }

            ketl_hir_var_id_t casted_var = trying_to_cast_rhs_to_lhs(p_context, lhs_type, expr_info, raw_var, false);

            return casted_var;
        }
    }
        
    // TODO more informative message
    errorf(expr_info.source_offset, expr_info.length, "Incompatible for cast types.");
    return push_temp_var(p_context, expr_info);
}

static void push_hir_assign_impl(ketl_parser_context* p_context, ketl_hir_tag_t op, ketl_hir_var_id_t lhs_var, ketl_hir_var_id_t rhs_var) {
    if (GET_VAR(lhs_var).uid == KETL_HIR_VAR_UID_LITERAL) {
        ketl_hir_expr_info_t expr_info = expr_info_merge(GET_VAR(lhs_var).expr_info, GET_VAR(rhs_var).expr_info);
        errorf(expr_info.source_offset, expr_info.length, "Can't assign to an r-value.");
        return;
    }

    rhs_var = trying_to_cast_rhs_to_lhs(p_context, GET_VAR(lhs_var).type, GET_VAR(lhs_var).expr_info, rhs_var, false);
    if (GET_VAR(rhs_var).type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return;
    }

    ketl_hir_builder_push_assign(&p_context->hir_builder, op, lhs_var, rhs_var);
}

static ketl_hir_var_id_t push_hir_append_value(ketl_parser_context* p_context, ketl_hir_var_id_t array_var, ketl_hir_var_id_t value_var) {
    ketl_hir_header_t header = {
        .tag = KETL_HIR_APPEND_VALUE,
        .file_symbol = p_context->s_filename,
    };
    ketl_hir_append_t instr = {
        .array_var = array_var,
        .append_var = value_var,
    };

    ketl_hir_builder_insert_instr(&p_context->hir_builder, header, (uint8_t*)&instr);
    return array_var;
}

static ketl_hir_var_id_t push_hir_append_array(ketl_parser_context* p_context, ketl_hir_var_id_t array_var, ketl_hir_var_id_t other_array_var) {
    ketl_hir_header_t header = {
        .tag = KETL_HIR_APPEND_ARRAY,
        .file_symbol = p_context->s_filename,
    };
    ketl_hir_append_t instr = {
        .array_var = array_var,
        .append_var = other_array_var,
    };

    ketl_hir_builder_insert_instr(&p_context->hir_builder, header, (uint8_t*)&instr);
    return array_var;
}

static ketl_hir_var_id_t push_append(ketl_parser_context* p_context, ketl_hir_var_id_t lhs_var, ketl_hir_var_id_t rhs_var) {
    ketl_hir_expr_info_t expr_info = expr_info_merge(GET_VAR(lhs_var).expr_info, GET_VAR(rhs_var).expr_info);
    
    if (GET_VAR(lhs_var).type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return push_temp_var(p_context, expr_info);
    }

    if (GET_VAR(lhs_var).type >= KETL_HIR_USED_TYPE_LAST) {
        errorf(expr_info.source_offset, expr_info.length, "Append must operate over array type value.");
        return push_temp_var(p_context, expr_info);
    }

    ketl_type* p_lhs_type = GET_TYPE(GET_VAR(lhs_var).type);
    
    if (p_lhs_type->kind != KETL_TYPE_ARRAY) {
        errorf(expr_info.source_offset, expr_info.length, "Append must operate over array type value.");
        return push_temp_var(p_context, expr_info);
    }

    if (GET_VAR(rhs_var).type >= KETL_HIR_USED_TYPE_LAST) {
        if (GET_VAR(rhs_var).type != KETL_HIR_USED_TYPE_LITERAL) {
            errorf(expr_info.source_offset, expr_info.length, "Append can't work with special types.");
            return push_temp_var(p_context, expr_info);
        }

        ketl_type* p_value_type = ((ketl_type_array*)p_lhs_type)->p_value_type;
        if (p_value_type->kind != KETL_TYPE_PRIMITIVE || !((ketl_type_primitive*)p_value_type)->is_numeric) {
            errorf(expr_info.source_offset, expr_info.length, "Append can't work with special types.");
            return push_temp_var(p_context, expr_info);
        }

        GET_VAR(rhs_var).type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_value_type);
    }

    ketl_type* p_rhs_type = GET_TYPE(GET_VAR(rhs_var).type);
    ketl_type_array* p_lhs_array_type = (ketl_type_array*) p_lhs_type;

    if (p_rhs_type->kind == KETL_TYPE_ARRAY && p_lhs_array_type->p_value_type != p_rhs_type) {
        ketl_type_array* p_rhs_array_type = (ketl_type_array*) p_rhs_type;

        if (p_lhs_array_type->p_value_type != p_rhs_array_type->p_value_type) {
            // TODO implicit casting
            errorf(expr_info.source_offset, expr_info.length, "Can't append different type.");
            return push_temp_var(p_context, expr_info);
        }

        return push_hir_append_array(p_context, lhs_var, rhs_var);
    } 

    if (p_rhs_type->kind == KETL_TYPE_CLASS && p_lhs_array_type->p_value_type->kind == KETL_TYPE_CLASS &&
        ketl_type_find_extended_class_offset(p_rhs_type, p_lhs_array_type->p_value_type) != (uint16_t)-1) {
        rhs_var = trying_to_cast_rhs_to_lhs(p_context, ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_lhs_array_type->p_value_type), 
            expr_info, rhs_var, false);
    } else if (p_lhs_array_type->p_value_type != p_rhs_type) {
        // TODO implicit casting
        errorf(expr_info.source_offset, expr_info.length, "Can't append different type.");
        return push_temp_var(p_context, expr_info);
    }

    return push_hir_append_value(p_context, lhs_var, rhs_var);
}

static ketl_hir_var_id_t push_hir_assign(ketl_parser_context* p_context, ketl_hir_tag_t op, ketl_hir_var_id_t lhs_var, ketl_hir_var_id_t rhs_var) {
    ketl_hir_var_t* p_lhs_var = &GET_VAR(lhs_var);
    if (p_lhs_var->uid == KETL_HIR_VAR_UID_LITERAL) {
        ketl_hir_expr_info_t expr_info = expr_info_merge(GET_VAR(lhs_var).expr_info, GET_VAR(rhs_var).expr_info);
        errorf(expr_info.source_offset, expr_info.length, "Can't assign to an l-value.");
        return push_temp_var(p_context, expr_info);
    }

    if (p_lhs_var->type == KETL_HIR_USED_TYPE_UNKNOWN || GET_VAR(rhs_var).type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return push_temp_var(p_context, expr_info_merge(GET_VAR(lhs_var).expr_info, GET_VAR(rhs_var).expr_info));
    }

    if (p_lhs_var->info != KETL_HIR_VAR_INFO_TEMP 
        && p_lhs_var->uid != KETL_HIR_VAR_UID_GLOBAL
        && p_lhs_var->uid != KETL_HIR_VAR_UID_FIELD) {
        // TODO will not work in a looping scenario without phi instruction at the begining of the block
        //lhs_var = ketl_hir_builder_increment_var_uid(&p_context->hir_builder, lhs_var);
        // TODO should not run during debug compilation
    }
    push_hir_assign_impl(p_context, op, lhs_var, rhs_var);

    // TODO should not return the same var. need to come up with semanticaly correct new var. full copy, same info? 
    return lhs_var;
}

static void push_hir_instr(ketl_parser_context* p_context, ketl_hir_tag_t tag) {
    ketl_hir_header_t header = {
        .tag = tag,
        .file_symbol = p_context->s_filename,
    };
    ketl_hir_builder_insert_instr(&p_context->hir_builder, header,  NULL);
}

static void push_hir_return_value(ketl_parser_context* p_context, ketl_hir_var_id_t var_id, ketl_hir_expr_info_t expr_info) {
    if (p_context->hir_builder.p_return_type != NULL) {
        var_id = trying_to_cast_rhs_to_lhs(p_context, ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_context->hir_builder.p_return_type), 
            expr_info, var_id, false);
    }

    if (GET_VAR(var_id).type != KETL_HIR_USED_TYPE_UNKNOWN) {
        ketl_type* p_type = GET_TYPE(GET_VAR(var_id).type);
        if (p_type->kind == KETL_TYPE_PRIMITIVE && p_type->size == 0) {
            errorf(expr_info.source_offset, expr_info.length, "Can't use void-like types for return value.");
            push_hir_instr(p_context, KETL_HIR_RETURN);
            return;
        }
    }
    ketl_hir_tag_t type_tag = GET_VAR(var_id).type == KETL_HIR_USED_TYPE_UNKNOWN ? KETL_HIR_UNDEF : ketl_hir_get_type_tag_from_type(GET_TYPE(GET_VAR(var_id).type));

    ketl_hir_header_t header = {
        .tag = KETL_HIR_RETURN_VALUE | type_tag,
        .file_symbol = p_context->s_filename,
    };
    ketl_hir_return_value_t instr = {
        .value_var = var_id,
    };
    ketl_hir_builder_insert_instr(&p_context->hir_builder, header, (uint8_t*)&instr);
}

static ketl_hir_var_id_t push_literal_id(ketl_parser_context* p_context, ketl_token_t literal) {
    // TODO decide how to update uids
    ketl_hir_symbol_offset_t name = push_symbol(p_context, literal);
    ketl_hir_var_id_t id_var = ketl_hir_builder_get_var(&p_context->hir_builder, p_context->p_namespace, name, name, token_extract_info(literal), true);
    ANN_ASSERT(GET_VAR(id_var).info != KETL_HIR_VAR_INFO_TEMP);
    if (GET_VAR(id_var).type == KETL_HIR_USED_TYPE_UNKNOWN) {
        _ketl_parse_undefined_vars_infos_t_bucket* p_bucket = _ketl_parse_undefined_vars_infos_t_get_or_insert_copy(&p_context->m_undef_vars_infos, GET_VAR(id_var).info, true);
        if (p_bucket->value) {
            errorf(literal.offset, literal.length, "Use of undeclared variable '%.*s'.", TOKEN_LENGTH(literal), TOKEN_STRING(literal));
            p_bucket->value = false;
        }
    }
    return id_var;
}

static ketl_hir_var_id_t push_array_index(ketl_parser_context* p_context, ketl_hir_var_id_t array_id, ketl_hir_var_id_t arg_id, ketl_hir_expr_info_t expr_info) {
    ketl_hir_var_id_t id_var = ketl_hir_builder_create_index_var(&p_context->hir_builder, array_id, arg_id, expr_info);
    return id_var;
}

static ketl_hir_var_id_t push_hir_variable_declaration(ketl_parser_context* p_context, ketl_token_t id_literal, ketl_hir_used_type_index_t type_index, ketl_hir_var_id_t init_var, ketl_hir_expr_info_t expr_info) {
    ketl_hir_var_id_t id_var;
    if (p_context->is_global_scope) {
        ketl_namespace_node* p_namespace_node = ketl_state_define_var(p_context->p_state, p_context->p_namespace, 
            TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal), GET_TYPE(type_index), p_context->export);
        id_var = ketl_hir_builder_get_global_var(&p_context->hir_builder, p_context->p_namespace, p_namespace_node, 
            push_symbol(p_context, id_literal), expr_info, type_index);
    } else {
        id_var = ketl_hir_builder_register_var(&p_context->hir_builder, p_context->p_namespace, 
            push_symbol(p_context, id_literal), expr_info, type_index);
    }

    if (GET_VAR(id_var).type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return id_var;
    }

    ketl_type* p_type = GET_TYPE(GET_VAR(id_var).type);
    if (p_type->kind == KETL_TYPE_PRIMITIVE && p_type->size == 0) {
        errorf(expr_info.source_offset, expr_info.length, "Can't use void-like types for variables");
        return id_var;
    }

    push_hir_assign_impl(p_context, KETL_HIR_ASSIGN, id_var, init_var);
    return id_var;
}

static ketl_hir_var_id_t push_hir_call(ketl_parser_context* p_context, ketl_hir_var_id_t callee_id, uint16_t arguments_count, ketl_hir_expr_info_t expr_info) {
    ketl_hir_var_t* p_callee = &GET_VAR(callee_id);

    if (p_callee->type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return push_temp_var(p_context, expr_info);
    }

    ketl_type* p_type = GET_TYPE(p_callee->type);

    if (p_type->kind != KETL_TYPE_FUNCTION && p_type->kind != KETL_TYPE_CFUNCTION) {
        errorf(expr_info.source_offset, expr_info.length, "Only functions can be called.");
        return push_temp_var(p_context, expr_info);
    }
    ketl_type_function* p_function_type = (ketl_type_function*)p_type;
    ketl_type_signature* p_function_signature = p_function_type->p_type_signature;

    if (p_function_signature->parameters_count - 1 != arguments_count) {
        errorf(expr_info.source_offset, expr_info.length, "Arguments count does not match function parameters count.");
        return push_temp_var(p_context, expr_info);
    }

    for (uint16_t i = 0; i < arguments_count; ++i) {
        ketl_hir_var_id_t* p_arg = &p_context->v_argument_stack.p_data[p_context->v_argument_stack.size - arguments_count + i];
        ketl_hir_expr_info_t arg_expr_info = GET_VAR(*p_arg).expr_info;
        if (GET_VAR(*p_arg).uid == KETL_HIR_VAR_UID_INDEX) {
            errorf(arg_expr_info.source_offset, arg_expr_info.length, "Can't use indexing as argument for call.");
        }
        *p_arg = trying_to_cast_rhs_to_lhs(p_context, ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_function_signature->a_parameters[i + 1].p_type), 
            arg_expr_info, *p_arg, false);
    }
    
    ketl_hir_header_t header = {
        .tag = KETL_HIR_CALL,
        .file_symbol = p_context->s_filename,
    };
    ketl_hir_var_id_t output_var = push_temp_var(p_context, expr_info); 
    ketl_hir_call_t instr = {
        .output_var = output_var,
        .callee = callee_id,
        .arguments_count = arguments_count,
    };

    ketl_hir_builder_insert_call(&p_context->hir_builder, header, &instr,
        // pass pointer to last 'arguments_count' elements and immidiatly cut 'arguments_count' tail
        p_context->v_argument_stack.p_data + (p_context->v_argument_stack.size -= arguments_count));
    return output_var;
}

static ketl_hir_var_id_t push_hir_new(ketl_parser_context* p_context, ketl_hir_var_id_t type_var, uint16_t arguments_count, ketl_hir_expr_info_t expr_info) {
    ketl_hir_header_t header = {
        .tag = KETL_HIR_NEW,
        .file_symbol = p_context->s_filename,
    };
    ketl_namespace_node* p_type_node = ketl_namespace_find_by_index(
        get_var_info(p_context, GET_VAR(type_var).info)->p_namespace,
        get_var_info(p_context, GET_VAR(type_var).info)->namespace_node_index);
    ketl_type* p_type = p_type_node->variable.p_pointer;
    ketl_hir_var_id_t output_var = push_temp_var_type(p_context, expr_info, ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_type)); 
    ketl_hir_new_t instr = {
        .output_var = output_var,
        .type_var = type_var,
        .arguments_count = arguments_count,
    };

    ketl_hir_builder_insert_new(&p_context->hir_builder, header, &instr,
        // pass pointer to last 'arguments_count' elements and immidiatly cut 'arguments_count' tail
        p_context->v_argument_stack.p_data + (p_context->v_argument_stack.size -= arguments_count));
    return output_var;
}

static ketl_hir_var_id_t push_hir_new_array(ketl_parser_context* p_context, ketl_hir_used_type_index_t value_type, ketl_hir_var_id_t count_var_id, ketl_hir_expr_info_t expr_info) {
    ketl_hir_header_t header = {
        .tag = KETL_HIR_CREATE_ARRAY,
        .file_symbol = p_context->s_filename,
    };
    ketl_type* p_value_type = GET_TYPE(value_type);
    ketl_hir_var_id_t output_var = push_temp_var_type(p_context, expr_info, 
        ketl_hir_builder_get_used_type_index(&p_context->hir_builder, ketl_state_get_array_type(p_context->p_state, p_value_type)));
    ketl_hir_create_array_t instr = {
        .output_var = output_var,
        .type = value_type,
        .count_var_id = count_var_id,
        .const_index = KETL_HIR_CONST_INDEX_NULL,
    };

    ketl_hir_builder_insert_instr(&p_context->hir_builder, header, (uint8_t*)&instr);
    return output_var;
}

static ketl_hir_var_id_t push_hir_new_string_literal(ketl_parser_context* p_context, ketl_token_t literal) {
    ketl_hir_header_t header = {
        .tag = KETL_HIR_CREATE_ARRAY,
        .file_symbol = p_context->s_filename,
    };
    ketl_hir_expr_info_t expr_info = token_extract_info(literal);
    ketl_hir_used_type_index_t value_type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, ketl_state_get_char_type(p_context->p_state));

    ketl_hir_var_id_t output_var = push_temp_var_type(p_context, expr_info, 
        ketl_hir_builder_get_used_type_index(&p_context->hir_builder, ketl_state_get_str_type(p_context->p_state)));

    const char* p_literal_str = TOKEN_STRING(literal);
    uint32_t length = literal.length; // can't use TOKEN_LENGTH, might be empty string literal

    uint32_t escape_characters_count = 0;
    for (uint32_t i = 0; i < length; ++i) {
        if (p_literal_str[i] == '\\' && (
            i == 0 || p_literal_str[i - 1] != '\\'
            )) {
            ++escape_characters_count;
        }
    }

    char a_size_buffer[16];
    uint32_t size_length = (uint32_t)snprintf(a_size_buffer, ANN_ARRAY_SIZE(a_size_buffer), "%"PRIu16, length - escape_characters_count);
    ketl_hir_var_id_t count_var_id = push_literal_number_symbol(p_context, push_symbol_string(p_context, a_size_buffer, size_length), expr_info);

    ketl_hir_const_index_t const_index = (ketl_hir_const_index_t)-1;
    if (length > 0) {
        const_index = ketl_hir_builder_push_string_literal(&p_context->hir_builder, literal);
    }

    ketl_hir_create_array_t instr = {
        .output_var = output_var,
        .type = value_type,
        .count_var_id = count_var_id,
        .const_index = const_index,
    };

    ketl_hir_builder_insert_instr(&p_context->hir_builder, header, (uint8_t*)&instr);
    return output_var;
}

static ketl_hir_var_id_t push_hir_new_slice(ketl_parser_context* p_context, ketl_hir_used_type_index_t value_type, ketl_hir_var_id_t mirror_var_id, ketl_hir_var_id_t start_var_id, ketl_hir_var_id_t end_var_id, ketl_hir_expr_info_t expr_info) {
    ketl_hir_header_t header = {
        .tag = KETL_HIR_CREATE_SLICE,
        .file_symbol = p_context->s_filename,
    };

    ketl_hir_var_id_t output_var = push_temp_var_type(p_context, expr_info, value_type);

    ketl_hir_create_slice_t instr = {
        .output_var = output_var,
        .type = value_type,
        .mirror_var_id = mirror_var_id,
        .start_var_id = start_var_id,
        .end_var_id = end_var_id,
    };

    ketl_hir_builder_insert_instr(&p_context->hir_builder, header, (uint8_t*)&instr);
    return output_var;
}

typedef ketl_hir_var_id_t(*ketl_parse_unary_rtl)(ketl_parser_context*);
typedef ketl_hir_var_id_t(*ketl_parse_ltr_infix)(ketl_parser_context*, ketl_hir_var_id_t);
typedef ketl_hir_var_id_t(*ketl_parse_rtl_infix)(ketl_parser_context*, ketl_hir_var_id_t, ketl_hir_var_id_t);

typedef uint8_t ketl_precedence;
enum {
    KETL_PREC_NONE,
    KETL_PREC_ASSIGNMENT,
    KETL_PREC_LOGICAL_OR,
    KETL_PREC_LOGICAL_AND,
    KETL_PREC_EQUALITY,
    KETL_PREC_COMPARISON,
    KETL_PREC_CONCAT,
    KETL_PREC_SHIFT,
    KETL_PREC_TERM,
    KETL_PREC_FACTOR,
    KETL_PREC_BITWISE_OR,
    KETL_PREC_BITWISE_XOR,
    KETL_PREC_BITWISE_AND,
    KETL_PREC_CALL,
    KETL_PREC_PRIMARY,
};

typedef uint8_t ketl_associativity;
enum {
    KETL_LTR,
    KETL_RTL,
};

ketl_associativity associativity[] = {
    [KETL_PREC_NONE] = KETL_LTR,
    [KETL_PREC_ASSIGNMENT] = KETL_RTL,
    [KETL_PREC_LOGICAL_OR] = KETL_LTR,
    [KETL_PREC_LOGICAL_AND] = KETL_LTR,
    [KETL_PREC_EQUALITY] = KETL_LTR,
    [KETL_PREC_COMPARISON] = KETL_LTR,
    [KETL_PREC_CONCAT] = KETL_LTR,
    [KETL_PREC_SHIFT] = KETL_LTR,
    [KETL_PREC_TERM] = KETL_LTR,
    [KETL_PREC_FACTOR] = KETL_LTR,
    [KETL_PREC_BITWISE_OR] = KETL_LTR,
    [KETL_PREC_BITWISE_XOR] = KETL_LTR,
    [KETL_PREC_BITWISE_AND] = KETL_LTR,
    [KETL_PREC_CALL] = KETL_LTR,
    [KETL_PREC_PRIMARY] = KETL_LTR,
};

ANN_DEFINE(ketl_parse_rule) {
    ketl_parse_unary_rtl f_prefix;
    ketl_parse_ltr_infix f_ltr_infix;
    ketl_parse_rtl_infix f_rtl_infix;
    ketl_precedence precedence;
};

static ketl_parse_rule* get_parse_rule(ketl_token_type token_type);
static ketl_hir_var_id_t parse_precedence(ketl_parser_context* p_context, ketl_precedence precedence);

static void token_advance(ketl_parser_context* p_context) {
    ANN_FOREVER {
        uint32_t current = ++p_context->p_lexer->token_iterator;

        if (current >= p_context->end_pos) break;
        if (TOKEN(current).type != KETL_TOKEN_TYPE_ERROR) break;

        // TODO move errorf to the lexer, make it informative
        errorf(TOKEN(current).offset, TOKEN(current).length, "Error in lexer.");
        ANN_ASSERT(false);
    }
}

static bool token_check(ketl_parser_context* p_context, ketl_token_type token_type) {
    return CURRENT_TOKEN(0).type == token_type;
}

static bool token_match(ketl_parser_context* p_context, ketl_token_type token_type) {
    if (!token_check(p_context, token_type)) return false;
    token_advance(p_context);
    return true;
}

static void token_consume(ketl_parser_context* p_context, ketl_token_type token_type, const char* message) {
    if (!token_match(p_context, token_type)) {
        errorf(CURRENT_TOKEN(1).offset + CURRENT_TOKEN(1).length, 0, "%s", message);
    }
}

static ketl_hir_var_id_t push_null_var(ketl_parser_context* p_context, ketl_hir_expr_info_t expr_info) {
    ketl_type* p_type = ketl_state_get_raw_type(p_context->p_state);
    ketl_hir_used_type_index_t type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_type);
    return push_literal_symbol_of_type(p_context, KETL_HIR_LITERAL_NULL, expr_info, type);
}

static ketl_hir_var_id_t parse_null(ketl_parser_context* p_context) {
    ketl_token_t null_token = CURRENT_TOKEN(1);
    return push_null_var(p_context, token_extract_info(null_token));
}

static ketl_hir_var_id_t parse_false(ketl_parser_context* p_context) {
    ketl_token_t false_token = CURRENT_TOKEN(1);
    return ketl_hir_builder_push_bool_var(&p_context->hir_builder, token_extract_info(false_token), false);
}


static ketl_hir_var_id_t parse_true(ketl_parser_context* p_context) {
    ketl_token_t true_token = CURRENT_TOKEN(1);
    return ketl_hir_builder_push_bool_var(&p_context->hir_builder, token_extract_info(true_token), true);
}

static ketl_hir_var_id_t parse_number(ketl_parser_context* p_context) {
    return push_literal_number(p_context, CURRENT_TOKEN(1));
}

static ketl_hir_var_id_t parse_string(ketl_parser_context* p_context) {
    ketl_token_t string_literal = CURRENT_TOKEN(1);
    return push_hir_new_string_literal(p_context, string_literal);
}

static ketl_hir_var_id_t parse_char(ketl_parser_context* p_context) {
    ketl_token_t literal = CURRENT_TOKEN(1);
    ketl_type* p_type = ketl_state_get_char_type(p_context->p_state);
    ketl_hir_used_type_index_t type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_type);
    return push_literal_symbol_of_type(p_context, push_symbol(p_context, literal), token_extract_info(literal), type);
}

static ketl_hir_var_id_t parse_identificator(ketl_parser_context* p_context) {
    return push_literal_id(p_context, CURRENT_TOKEN(1));
}

static ketl_type* parse_type(ketl_parser_context* p_context) {
    ketl_variable_type_info_t a_arg_types[16] = {0};
    uint8_t arg_types_count = 0;
    if (token_match(p_context, KETL_TOKEN_TYPE_PARENTHESIS_LEFT)) {
        do {
            if (token_check(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT)) {
                break;
            }

            a_arg_types[++arg_types_count].p_type = parse_type(p_context);
        } while (token_match(p_context, KETL_TOKEN_TYPE_COMMA));

        token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')'.");

        if (token_match(p_context, KETL_TOKEN_TYPE_DOUBLE_ARROW_RIGHT)) {
            a_arg_types[0].p_type = parse_type(p_context);
            
            ketl_function_parameters function_parameters = {
                .p_parameters = a_arg_types,
                .parameters_count = arg_types_count + 1,
            };
            ketl_type* function_type = ketl_state_get_cfunction_type(p_context->p_state, &function_parameters);
            return function_type;
        }

        ANN_ASSERT(false);
    }

    token_advance(p_context);
    ketl_token_t id_literal = CURRENT_TOKEN(1);
    
    ketl_atomic_string s_id = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal));
    ketl_namespace_node* p_node = ketl_namespace_find(p_context->p_namespace, s_id);

    ANN_FOREVER {
        switch (CURRENT_TOKEN(0).type) {
            case KETL_TOKEN_TYPE_SQUARE_LEFT: {
                ANN_ASSERT(p_node == NULL || p_node->variable.kind == KETL_VARIABLE_TYPE);
                if (p_node == NULL) {
                    errorf(id_literal.offset, id_literal.length, "'%.*s' is not a known type.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal));
                    return NULL;
                }

                ketl_type* p_type = p_node->variable.p_pointer;

                while (token_match(p_context, KETL_TOKEN_TYPE_SQUARE_LEFT)) {
                    ketl_type* p_index_type = NULL;
                    if (!token_match(p_context, KETL_TOKEN_TYPE_SQUARE_RIGHT)) {
                        p_index_type = parse_type(p_context);
                        token_consume(p_context, KETL_TOKEN_TYPE_SQUARE_RIGHT, "Expected ']'.");
                    }

                    if (p_index_type != NULL) {
                        // TODO implement dict
                        (void)p_index_type;
                        ANN_ASSERT(false);
                    }

                    p_type = ketl_state_get_array_type(p_context->p_state, p_type);
                }

                return p_type;

                //continue;
            }
            case KETL_TOKEN_TYPE_DOT: {
                token_advance(p_context); // '.'

                id_literal = CURRENT_TOKEN(0);
                token_advance(p_context);

                if (p_node == NULL) {
                    continue;
                }

                ANN_SWITCH_STRICT(p_node->variable.kind) {
                    case KETL_VARIABLE_TYPE: {
                        ketl_type* p_type = p_node->variable.p_pointer;
                        ANN_ASSERT(p_type->kind == KETL_TYPE_CLASS);
                        ketl_namespace* p_namespace = &((ketl_type_class*)p_type)->namespace;

                        s_id = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal));
                        p_node = ketl_namespace_find(p_namespace, s_id);
                        break;
                    }
                    case KETL_VARIABLE_NAMESPACE: {
                        ketl_namespace* p_namespace = p_node->variable.p_pointer;

                        s_id = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal));
                        p_node = ketl_namespace_find(p_namespace, s_id);
                        break;
                    }
                }

                continue;
            }
        }

        break;
    }

    ketl_type* p_type = p_node && p_node->variable.kind == KETL_VARIABLE_TYPE ? p_node->variable.p_pointer : NULL;

    if (p_type == NULL) {
        errorf(id_literal.offset, id_literal.length, "'%.*s' is not a known type.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal));
    }
    return p_type;
}

static ketl_hir_used_type_index_t parse_type_as_index(ketl_parser_context* p_context) {
    return  ketl_hir_builder_get_used_type_index(&p_context->hir_builder, parse_type(p_context));
}

static ketl_hir_var_id_t parse_expression(ketl_parser_context* p_context);
static ketl_statement_info parse_statement(ketl_parser_context* p_context);
static ketl_statement_info parse_declaration(ketl_parser_context* p_context); 

static ketl_hir_var_id_t parse_grouping(ketl_parser_context* p_context) {
    ketl_hir_var_id_t expr = parse_expression(p_context);
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after expression.");
    return expr;
}

static uint16_t parse_argument_list(ketl_parser_context* p_context) {
    uint16_t argument_count = 0;
    if (!token_check(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT)) {
        do {
            push_hir_argument(p_context, parse_expression(p_context));
            ++argument_count;
        } while (token_match(p_context, KETL_TOKEN_TYPE_COMMA));
    }
    
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after arguments.");
    return argument_count;
}

static ketl_hir_var_id_t parse_call(ketl_parser_context* p_context, ketl_hir_var_id_t callee) {
    uint16_t argument_count = parse_argument_list(p_context);
    ketl_hir_expr_info_t expr_info = expr_info_merge(GET_VAR(callee).expr_info, token_extract_info(CURRENT_TOKEN(1)));

    ketl_hir_var_t* p_callee = &GET_VAR(callee);
    if (p_callee->type == KETL_HIR_USED_TYPE_META) {
        ketl_hir_var_info_t* p_callee_info = get_var_info(p_context, p_callee->info);
        ketl_namespace_node* p_callee_node = ketl_namespace_find_by_index(p_callee_info->p_namespace, p_callee_info->namespace_node_index);
        if (p_callee_node->variable.kind != KETL_VARIABLE_TYPE) {
            errorf(expr_info.source_offset, expr_info.length, "Trying to call an improper object.");
            return push_temp_var(p_context, expr_info);
        }
        ketl_type* p_callee_type = p_callee_node->variable.p_pointer;
        if (p_callee_type->kind != KETL_TYPE_CLASS) {
            errorf(expr_info.source_offset, expr_info.length, "Only class objects can be constructed through calling a type.");
            return push_temp_var(p_context, expr_info);
        }
        
        return push_hir_new(p_context, callee, argument_count, expr_info);
    } else {
        return push_hir_call(p_context, callee, argument_count, expr_info);
    }
}

static ketl_hir_var_id_t push_access_class_namespace(ketl_parser_context* p_context, ketl_token_t id_literal, ketl_hir_expr_info_t expr_info, ketl_type* p_type, bool force) {
    ANN_ASSERT(p_type && p_type->kind == KETL_TYPE_CLASS);

    ketl_namespace* p_namespace = &((ketl_type_class*)p_type)->namespace;

    ketl_hir_var_id_t var_id = ketl_hir_builder_get_var(&p_context->hir_builder, p_namespace, push_symbol(p_context, id_literal), 
        push_symbol_string(p_context, p_context->p_lexer->p_source + expr_info.source_offset, expr_info.length), expr_info, force);
    ANN_ASSERT(!force || GET_VAR(var_id).info != KETL_HIR_VAR_INFO_TEMP);
    return var_id;
}

static ketl_hir_var_id_t find_field(ketl_parser_context* p_context, ketl_hir_var_id_t object_id, ketl_token_t id_literal, ketl_hir_expr_info_t expr_info, bool force) {
    ketl_type* p_object_type = GET_TYPE(GET_VAR(object_id).type);

    ketl_atomic_string s_field_name = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal));
    ketl_type* p_field_type = ketl_type_find_field_type(p_object_type, s_field_name, p_context->p_state);

    if (p_field_type != NULL) {
        ketl_hir_used_type_index_t field_type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_field_type);
        
        ketl_hir_var_id_t id_var = ketl_hir_builder_create_field_var(&p_context->hir_builder, object_id,
            push_symbol(p_context, id_literal), expr_info, field_type);
        return id_var;
    }

    if (p_object_type->kind == KETL_TYPE_CLASS) {
        ketl_hir_var_id_t var_id = push_access_class_namespace(p_context, id_literal, expr_info, p_object_type, force);
        return var_id;
    }

    if (p_object_type->kind == KETL_TYPE_ARRAY && ketl_str_is_equal_n("clear", TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal))) {
        ketl_namespace* p_namespace = &p_context->p_state->secret_namespace;

        ketl_hir_symbol_offset_t symbol = push_symbol_string(p_context, "array_clear", 11);
        ketl_hir_var_id_t var_id = ketl_hir_builder_get_var(&p_context->hir_builder, p_namespace, symbol, symbol, expr_info, force);
        ANN_ASSERT(!force || GET_VAR(var_id).info != KETL_HIR_VAR_INFO_TEMP);
        return var_id;
    }

    if (p_object_type->kind == KETL_TYPE_ARRAY && ketl_str_is_equal_n("pop", TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal))) {
        ketl_namespace* p_namespace = &p_context->p_state->secret_namespace;

        ketl_hir_symbol_offset_t symbol = push_symbol_string(p_context, "array_pop", 9);
        ketl_hir_var_id_t var_id = ketl_hir_builder_get_var(&p_context->hir_builder, p_namespace, symbol, symbol, expr_info, force);
        ANN_ASSERT(!force || GET_VAR(var_id).info != KETL_HIR_VAR_INFO_TEMP);
        return var_id;
    }

    if (force) {
        return ketl_hir_builder_create_temp_var(&p_context->hir_builder, expr_info, KETL_HIR_USED_TYPE_UNKNOWN);
    } else {
        return -1;
    }
}

static ketl_hir_var_id_t parse_dot_operator(ketl_parser_context* p_context, ketl_hir_var_id_t lhs) {
    ketl_hir_var_t* p_object = &GET_VAR(lhs);
    token_consume(p_context, KETL_TOKEN_TYPE_ID, "Expected id after access operator.");
    ketl_token_t id_literal = CURRENT_TOKEN(1);

    if (p_object->type == KETL_HIR_USED_TYPE_META) {
        ketl_hir_expr_info_t expr_info = expr_info_merge(p_object->expr_info, token_extract_info(id_literal));

        ketl_namespace_node* p_namespace_node = ketl_namespace_find_by_index(
            get_var_info(p_context, p_object->info)->p_namespace, get_var_info(p_context, p_object->info)->namespace_node_index);
        
        if (p_namespace_node->variable.kind == KETL_VARIABLE_NAMESPACE) { 
            ketl_namespace* p_namespace = p_namespace_node->variable.p_pointer;
            
            ketl_hir_var_id_t id_var = ketl_hir_builder_get_var(&p_context->hir_builder, p_namespace, push_symbol(p_context, id_literal), 
                push_symbol_string(p_context, p_context->p_lexer->p_source + expr_info.source_offset, expr_info.length), expr_info, true);
            ANN_ASSERT(GET_VAR(id_var).info != KETL_HIR_VAR_INFO_TEMP);
            if (GET_VAR(id_var).type == KETL_HIR_USED_TYPE_UNKNOWN) {
                _ketl_parse_undefined_vars_infos_t_bucket* p_bucket = _ketl_parse_undefined_vars_infos_t_get_or_insert_copy(&p_context->m_undef_vars_infos, GET_VAR(id_var).info, true);
                if (p_bucket->value) {
                    errorf(id_literal.offset, id_literal.length, "Use of undeclared variable '%.*s' from module '%s'.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal),
                        ketl_atomic_strings_get_pointer(&p_context->p_state->atomic_strings, p_namespace->s_name));
                    p_bucket->value = false;
                }
            }

            return id_var;
        }

        if (p_namespace_node->variable.kind == KETL_VARIABLE_TYPE) {
            ketl_type* p_type = p_namespace_node->variable.p_pointer;

            if (p_type->kind == KETL_TYPE_CLASS) {
                ketl_hir_var_id_t var_id = push_access_class_namespace(p_context, id_literal, expr_info, p_type, true);
                if (GET_VAR(var_id).type == KETL_HIR_USED_TYPE_UNKNOWN) {
                    _ketl_parse_undefined_vars_infos_t_bucket* p_bucket = _ketl_parse_undefined_vars_infos_t_get_or_insert_copy(&p_context->m_undef_vars_infos, GET_VAR(var_id).info, true);
                    if (p_bucket->value) {
                        errorf(id_literal.offset, id_literal.length, "Use of undeclared entity '%.*s' from class '%s'.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal),
                            ketl_atomic_strings_get_pointer(&p_context->p_state->atomic_strings, ((ketl_type_class*)p_type)->s_name));
                        p_bucket->value = false;
                    }
                }
                return var_id;
            }

            if (p_type->kind == KETL_TYPE_ENUM) {
                ketl_variable constant = ketl_type_find_enum_constant_value(p_type, ketl_atomic_strings_get(&p_context->p_state->atomic_strings, 
                    TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal)));
                
                if (constant.kind == KETL_VARIABLE_NONE) {
                    errorf(id_literal.offset, id_literal.length, "Use of undeclared constant '%.*s' from enum '%s'.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal),
                        ketl_atomic_strings_get_pointer(&p_context->p_state->atomic_strings, ((ketl_type_enum*)p_type)->s_name));
                    return push_temp_var(p_context, expr_info);
                }

                char a_symbol[256];
                uint32_t symbol_length;

                ANN_SWITCH_STRICT(constant.kind) {
                    case KETL_VARIABLE_INT8:
                    case KETL_VARIABLE_INT16:
                    case KETL_VARIABLE_INT32:
                    case KETL_VARIABLE_INT64:
                        symbol_length = snprintf(a_symbol, ANN_ARRAY_SIZE(a_symbol), "%"PRIi64, constant.int64);
                        break;
                    case KETL_VARIABLE_UINT8:
                    case KETL_VARIABLE_UINT16:
                    case KETL_VARIABLE_UINT32:
                    case KETL_VARIABLE_UINT64:
                        symbol_length = snprintf(a_symbol, ANN_ARRAY_SIZE(a_symbol), "%"PRIu64, constant.uint64);
                        break;
                }

                return push_literal_symbol_of_type(p_context, push_symbol_string(p_context, a_symbol, symbol_length), expr_info, 
                    ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_type));
            } 

            ANN_ASSERT(false && "This type does not have this field");
        }

        ANN_ASSERT(false && "Trying to field access no-type and no-namespace");
        return push_temp_var(p_context, expr_info);
    } else {
        ketl_hir_var_id_t object_id = lhs;

        ketl_hir_expr_info_t expr_info = expr_info_merge(GET_VAR(object_id).expr_info, token_extract_info(id_literal));
        
        if (GET_VAR(object_id).type == KETL_HIR_USED_TYPE_UNKNOWN) {
            return push_temp_var(p_context, expr_info);
        }

        ketl_type* p_object_type = GET_TYPE(GET_VAR(object_id).type);

        if (p_object_type->kind != KETL_TYPE_CLASS && p_object_type->kind != KETL_TYPE_ARRAY) {
            errorf(expr_info.source_offset, expr_info.length, "Fields and properties through access parameter only allowed for class and array objects.");
            return ketl_hir_builder_create_temp_var(&p_context->hir_builder, expr_info, KETL_HIR_USED_TYPE_UNKNOWN);
        }

        ketl_hir_var_id_t field_id = find_field(p_context, object_id, id_literal, expr_info, true);
        if (GET_VAR(field_id).type == KETL_HIR_USED_TYPE_UNKNOWN) {
            _ketl_parse_undefined_vars_infos_t_bucket* p_bucket = _ketl_parse_undefined_vars_infos_t_get_or_insert_copy(&p_context->m_undef_vars_infos, GET_VAR(field_id).info, true);
            if (p_bucket->value) {
                errorf(id_literal.offset, id_literal.length, "Use of undeclared entity '%.*s' from %s '%s'.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal),
                    p_object_type->kind == KETL_TYPE_CLASS ? "class" : "array", 
                    ketl_atomic_strings_get_pointer(&p_context->p_state->atomic_strings, ((ketl_type_class*)p_object_type)->s_name));
                p_bucket->value = false;
            }
        }

        return field_id;
    }
}

static ketl_hir_var_id_t parse_arrow_operator(ketl_parser_context* p_context, ketl_hir_var_id_t lhs) {
    token_consume(p_context, KETL_TOKEN_TYPE_ID, "Expected id after arrow operator.");
    ketl_token_t id_literal = CURRENT_TOKEN(1);
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, "Expected '(' after id of a arrow operator.");

    push_hir_argument(p_context, lhs);
    uint16_t argument_count = 1;
    if (!token_check(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT)) {
        do {
            push_hir_argument(p_context, parse_expression(p_context));
            ++argument_count;
        } while (token_match(p_context, KETL_TOKEN_TYPE_COMMA));
    }
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after arguments.");

    ketl_hir_var_id_t callee = -1;
    ketl_hir_expr_info_t expr_info = expr_info_merge(GET_VAR(lhs).expr_info, token_extract_info(CURRENT_TOKEN(1)));
    
    if (GET_VAR(lhs).type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return push_temp_var(p_context, expr_info);
    }

    if (GET_VAR(lhs).type == KETL_HIR_USED_TYPE_META) {
        errorf(expr_info.source_offset, expr_info.length, "Can't use arrow operator with types or namespaces.");
        return push_temp_var(p_context, expr_info);
    }

    ketl_type* p_object_type;

    if (GET_VAR(lhs).type == KETL_HIR_USED_TYPE_LITERAL) {
        p_object_type = ketl_state_get_i64(p_context->p_state);
    } else {
        p_object_type = GET_TYPE(GET_VAR(lhs).type);
    }

    if (p_object_type->kind == KETL_TYPE_CLASS || p_object_type->kind == KETL_TYPE_ARRAY) {
        callee = find_field(p_context, lhs, id_literal, expr_info, false);
    }

    if (callee == (ketl_hir_var_id_t)(-1)) {
        // check global namespace
        ketl_hir_symbol_offset_t name = push_symbol(p_context, id_literal);
        callee = ketl_hir_builder_get_var(&p_context->hir_builder, p_context->p_namespace, name, name, expr_info, true);
    }

    ANN_ASSERT(GET_VAR(callee).info != KETL_HIR_VAR_INFO_TEMP);
    if (GET_VAR(callee).type == KETL_HIR_USED_TYPE_UNKNOWN) {
        _ketl_parse_undefined_vars_infos_t_bucket* p_bucket = _ketl_parse_undefined_vars_infos_t_get_or_insert_copy(&p_context->m_undef_vars_infos, GET_VAR(callee).info, true);
        if (p_bucket->value) {
            errorf(id_literal.offset, id_literal.length, "Use of undeclared variable '%.*s'.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal));
            p_bucket->value = false;
        }
        return callee;
    }

    return push_hir_call(p_context, callee, argument_count, expr_info);
}

static ketl_hir_var_id_t parse_dollar_prefix(ketl_parser_context* p_context) {
    token_consume(p_context, KETL_TOKEN_TYPE_ID, "Expected id after '$' prefix.");
    ketl_token_t id_literal = CURRENT_TOKEN(1);

    ketl_hir_expr_info_t expr_info = expr_info_merge(token_extract_info(CURRENT_TOKEN(2)), token_extract_info(id_literal));

    /*
    if (ketl_str_is_equal_n("module_init", TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal))) {
        ketl_atomic_string s_module_name = p_context->p_state->active_module_name;
        if (s_module_name == KETL_ATOMIC_STRING_EMPTY) {
            // TODO something
            ANN_ASSERT(false);
        }

        ketl_module_t* p_module = &ketl_modules_t_get_or_null(&p_context->p_state->modules, s_module_name)->value;
    }
    */

    errorf(expr_info.source_offset, expr_info.length, "Unknown '$' property '%.*s'.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal));
    return push_temp_var(p_context, expr_info);
}

static ketl_hir_var_id_t parse_dollar_operator(ketl_parser_context* p_context, ketl_hir_var_id_t lhs) {
    ketl_hir_var_t* p_object = &GET_VAR(lhs);
    token_consume(p_context, KETL_TOKEN_TYPE_ID, "Expected id after meta access operator.");
    ketl_token_t id_literal = CURRENT_TOKEN(1);

    ketl_hir_expr_info_t expr_info = expr_info_merge(p_object->expr_info, token_extract_info(id_literal));

    if (p_object->type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return push_temp_var(p_context, expr_info);
    }

    if (p_object->type == KETL_HIR_USED_TYPE_META) {
        ketl_namespace_node* p_namespace_node = ketl_namespace_find_by_index(
            get_var_info(p_context, p_object->info)->p_namespace, get_var_info(p_context, p_object->info)->namespace_node_index);

        if (p_namespace_node->variable.kind != KETL_VARIABLE_TYPE) {
            errorf(expr_info.source_offset, expr_info.length, "Namespaces doesn't support meta properties.");
            return push_temp_var(p_context, expr_info);
        }

        ketl_type* p_type = p_namespace_node->variable.p_pointer;
        
        if (ketl_str_is_equal_n("size", TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal))) {
            uint64_t type_size = ketl_type_get_size(p_type);

            char a_buffer[256];
            uint32_t length = (uint32_t)snprintf(a_buffer, ANN_ARRAY_SIZE(a_buffer), "%"PRIu64, type_size);

            return push_literal_number_symbol(p_context, push_symbol_string(p_context, a_buffer, length), expr_info);
        }

        if (ketl_str_is_equal_n("count", TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal))) {
            if (p_type->kind != KETL_TYPE_ENUM) {
                errorf(expr_info.source_offset, expr_info.length, "Only enum has meta property '%.*s'", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal));
                return push_temp_var(p_context, expr_info);
            }

            uint64_t type_size = ((ketl_type_enum*)p_type)->constants_count;

            char a_buffer[256];
            uint32_t length = (uint32_t)snprintf(a_buffer, ANN_ARRAY_SIZE(a_buffer), "%"PRIu64, type_size);

            return push_literal_number_symbol(p_context, push_symbol_string(p_context, a_buffer, length), expr_info);
        }

        errorf(expr_info.source_offset, expr_info.length, "There is no meta property '%.*s' of type.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal));
        return push_temp_var(p_context, expr_info);
    }

    if (ketl_str_is_equal_n("cast", TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal))) {
        token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, "Expected '(' after 'cast' meta property.");
        ketl_type* p_cast_to_type = parse_type(p_context);
        token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after 'cast' meta property parameter.");

        expr_info = expr_info_merge(expr_info, token_extract_info(CURRENT_TOKEN(1)));

        if (p_cast_to_type == NULL) {
            return push_temp_var(p_context, expr_info);
        }

        if (p_object->type == KETL_HIR_USED_TYPE_LITERAL || 
            (GET_TYPE(p_object->type)->kind == KETL_TYPE_PRIMITIVE && ((ketl_type_primitive*)GET_TYPE(p_object->type))->is_numeric) ||
            ketl_type_is_char_type(GET_TYPE(p_object->type))) {
            // casting to integer to str
            if (p_cast_to_type->kind == KETL_TYPE_ARRAY && 
                ketl_type_is_char_type(((ketl_type_array*)p_cast_to_type)->p_value_type)) {
                ketl_hir_var_id_t output_var = trying_to_cast_rhs_to_lhs(p_context, 
                    ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_cast_to_type), expr_info, lhs, true);
                return output_var;
            }
  
            if (!(p_object->type != KETL_HIR_USED_TYPE_LITERAL && ketl_type_is_u64_type(GET_TYPE(p_object->type)) && ketl_type_is_raw_type(p_cast_to_type)) &&
                !((p_cast_to_type->kind == KETL_TYPE_PRIMITIVE && ((ketl_type_primitive*)p_cast_to_type)->is_numeric) ||
                ketl_type_is_char_type(p_cast_to_type) || 
                p_cast_to_type->kind == KETL_TYPE_ENUM)) {
                errorf(expr_info.source_offset, expr_info.length, "Casting of primitives numeric types supported only to primitive numeric types.");
                return push_temp_var(p_context, expr_info);
            }
        }

        if (p_object->type < KETL_HIR_USED_TYPE_LAST && ketl_type_is_raw_type(GET_TYPE(p_object->type))) {
            if (p_cast_to_type->kind != KETL_TYPE_ARRAY && p_cast_to_type->kind != KETL_TYPE_CLASS &&
                !ketl_type_is_u64_type(p_cast_to_type)) {
                errorf(expr_info.source_offset, expr_info.length, "Casting of raw type supported only to classes or arrays.");
                return push_temp_var(p_context, expr_info);
            }
        }

        if (p_object->type == KETL_HIR_USED_TYPE_LITERAL) {
            p_object->type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_cast_to_type);
            p_object->expr_info = expr_info_merge(p_object->expr_info, token_extract_info(CURRENT_TOKEN(1)));
            return lhs;
        } else {
            ketl_hir_var_id_t output_var = trying_to_cast_rhs_to_lhs(p_context, 
                ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_cast_to_type), expr_info, lhs, true);
            return output_var;
        }
    }

    errorf(expr_info.source_offset, expr_info.length, "There is no meta property '%.*s'.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal));
    return push_temp_var(p_context, expr_info);
}

static ketl_hir_var_id_t parse_indexing(ketl_parser_context* p_context, ketl_hir_var_id_t var_id) {
    ketl_type* p_type = NULL;
    if (token_match(p_context, KETL_TOKEN_TYPE_SQUARE_RIGHT)) {
        if (GET_VAR(var_id).type != KETL_HIR_USED_TYPE_META) {
            ketl_hir_expr_info_t expr_info = expr_info_merge(GET_VAR(var_id).expr_info, token_extract_info(CURRENT_TOKEN(1)));
            errorf(expr_info.source_offset, expr_info.length, "Unsupported target for empty index operator.");
            return push_temp_var(p_context, expr_info);
        }
        ketl_namespace_node* p_node_type = ketl_namespace_find_by_index(
            get_var_info(p_context, GET_VAR(var_id).info)->p_namespace,
            get_var_info(p_context, GET_VAR(var_id).info)->namespace_node_index
        );
        ANN_ASSERT(p_node_type && p_node_type->variable.kind == KETL_VARIABLE_TYPE);
        p_type = p_node_type->variable.p_pointer;

        p_type = ketl_state_get_array_type(p_context->p_state, p_type);

        if (!token_match(p_context, KETL_TOKEN_TYPE_SQUARE_LEFT)) {
            ANN_ASSERT(false);
        }
    }

    ketl_hir_var_id_t expr_id = parse_expression(p_context);

    if (token_match(p_context, KETL_TOKEN_TYPE_COLON)) {
        ANN_ASSERT(p_type == NULL);

        ketl_hir_var_id_t start_id = expr_id;
        ketl_hir_var_id_t end_id = parse_expression(p_context);

        token_consume(p_context, KETL_TOKEN_TYPE_SQUARE_RIGHT, "Expected ']' after expression.");
        ketl_hir_expr_info_t expr_info = expr_info_merge(GET_VAR(var_id).expr_info, token_extract_info(CURRENT_TOKEN(1)));

        ANN_ASSERT(GET_VAR(var_id).type != KETL_HIR_USED_TYPE_UNKNOWN && GET_TYPE(GET_VAR(var_id).type)->kind == KETL_TYPE_ARRAY);

        ketl_hir_var_id_t output_var = push_hir_new_slice(p_context, 
            ketl_hir_builder_get_used_type_index(&p_context->hir_builder, GET_TYPE(GET_VAR(var_id).type)), var_id, start_id, end_id, expr_info);

        return output_var;
    }

    token_consume(p_context, KETL_TOKEN_TYPE_SQUARE_RIGHT, "Expected ']' after expression.");

    ketl_hir_expr_info_t expr_info = expr_info_merge(GET_VAR(var_id).expr_info, token_extract_info(CURRENT_TOKEN(1)));

    if (GET_VAR(var_id).type == KETL_HIR_USED_TYPE_META) {
        ketl_type* p_value_type = p_type;
        if (p_value_type == NULL) {
            ketl_namespace_node* p_value_type_node = ketl_namespace_find_by_index(
                get_var_info(p_context, GET_VAR(var_id).info)->p_namespace, 
                get_var_info(p_context, GET_VAR(var_id).info)->namespace_node_index);
            p_value_type = p_value_type_node->variable.p_pointer;
        }
        ketl_hir_var_id_t id_var = push_hir_new_array(p_context, ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_value_type), expr_id, expr_info);

        if (!token_match(p_context, KETL_TOKEN_TYPE_CURLY_LEFT)) {
            return id_var;
        }

        ketl_hir_used_type_index_t size_type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, ketl_state_get_u64(p_context->p_state)); 
        ketl_hir_var_id_t index_var = -1;
        do {
            if (token_check(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT)) {
                break;
            }

            ketl_hir_var_id_t value_var = parse_expression(p_context);

            if (token_match(p_context, KETL_TOKEN_TYPE_DOUBLE_ARROW_RIGHT)) {
                index_var = value_var;
                value_var = parse_expression(p_context);
            } else if (index_var == (ketl_hir_var_id_t)-1) {
                index_var = ketl_hir_builder_create_temp_var(&p_context->hir_builder, expr_info, size_type);
                ketl_hir_var_id_t zero_var = ketl_hir_builder_get_literal(&p_context->hir_builder, KETL_HIR_LITERAL_NULL, expr_info, size_type);
                ketl_hir_builder_push_assign(&p_context->hir_builder, KETL_HIR_ASSIGN, index_var, zero_var);
            } else {
                ketl_hir_var_id_t one_var = ketl_hir_builder_get_literal(&p_context->hir_builder, 
                    (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_context->hir_builder.symbols, "1", 1), expr_info, GET_VAR(index_var).type);
                index_var = push_hir_binary_op(p_context, KETL_HIR_PLUS, index_var, one_var, expr_info);
            }

            ketl_hir_var_id_t indexed_var = push_array_index(p_context, id_var, index_var, GET_VAR(value_var).expr_info);

            push_hir_assign(p_context, KETL_HIR_ASSIGN, indexed_var, value_var);
        } while (token_match(p_context, KETL_TOKEN_TYPE_COMMA));
        
        token_consume(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT, "Expected '}' after array initialization values.");
        return id_var;
    } else {
        if (GET_VAR(expr_id).type == KETL_HIR_USED_TYPE_LITERAL) {
            return push_array_index(p_context, var_id, expr_id, expr_info);
        }

        if (GET_VAR(expr_id).type >= KETL_HIR_USED_TYPE_LAST) {
            errorf(expr_info.source_offset, expr_info.length, "Can't use non-integer type for indexing an array.");
            return push_temp_var(p_context, expr_info);
        }

        ketl_type* p_type = GET_TYPE(GET_VAR(expr_id).type);
        if (p_type->kind == KETL_TYPE_ENUM) {
            p_type = (ketl_type*)((ketl_type_enum*)p_type)->p_parent_primitive;
        }
        if (p_type->kind != KETL_TYPE_PRIMITIVE) {
            errorf(expr_info.source_offset, expr_info.length, "Can't use non-integer type for indexing an array.");
            return push_temp_var(p_context, expr_info);
        }

        ketl_type_primitive* p_primitive_type = (ketl_type_primitive*)p_type;
        if (!p_primitive_type->is_integer) {
            errorf(expr_info.source_offset, expr_info.length, "Can't use non-integer type for indexing an array.");
            return push_temp_var(p_context, expr_info);
        }
        return push_array_index(p_context, var_id, expr_id, expr_info);
    }
}

static ketl_hir_var_id_t parse_unary_rtl(ketl_parser_context* p_context) {
    ketl_token_t token = CURRENT_TOKEN(1);
    ketl_token_type token_type = token.type;
    ketl_hir_var_id_t rhs = parse_precedence(p_context, KETL_PREC_CALL);

    ANN_SWITCH_STRICT (token_type) {
        case KETL_TOKEN_TYPE_LOGICAL_NOT: return push_hir_unary_op(p_context, KETL_HIR_LOGICAL_NOT, rhs, token_extract_info(token));
        case KETL_TOKEN_TYPE_BITWISE_NOT: return push_hir_unary_op(p_context, KETL_HIR_BITWISE_NOT, rhs, token_extract_info(token));

        case KETL_TOKEN_TYPE_PLUS:        return push_hir_unary_op(p_context, KETL_HIR_UNARY_PLUS,  rhs, token_extract_info(token));
        case KETL_TOKEN_TYPE_MINUS:       return push_hir_unary_op(p_context, KETL_HIR_UNARY_MINUS, rhs, token_extract_info(token));
    }
}

static ketl_hir_var_id_t push_hir_concat(ketl_parser_context* p_context, ketl_hir_var_id_t lhs, ketl_hir_var_id_t rhs, ketl_hir_expr_info_t expr_info) {
    if (GET_VAR(lhs).type == KETL_HIR_USED_TYPE_UNKNOWN || GET_VAR(rhs).type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return push_temp_var(p_context, expr_info);
    }

    ketl_type* p_lhs_type = GET_TYPE(GET_VAR(lhs).type);
    ketl_type* p_rhs_type = GET_TYPE(GET_VAR(rhs).type);

    ketl_type* p_result_value_type = NULL;

    if (p_lhs_type->kind == KETL_TYPE_ARRAY && p_rhs_type->kind == KETL_TYPE_ARRAY) {
        if (((ketl_type_array*)p_lhs_type)->p_value_type != ((ketl_type_array*)p_rhs_type)->p_value_type) {
            errorf(expr_info.source_offset, expr_info.length, "Can't concat arrays of different value types.");
            return push_temp_var(p_context, expr_info);
        }
        
        p_result_value_type = ((ketl_type_array*)p_lhs_type)->p_value_type;
    } else if (p_lhs_type->kind == KETL_TYPE_ARRAY) {
        if (((ketl_type_array*)p_lhs_type)->p_value_type != p_rhs_type) {
            errorf(expr_info.source_offset, expr_info.length, "Can't concat array with different value type.");
            return push_temp_var(p_context, expr_info);
        }
        
        p_result_value_type = ((ketl_type_array*)p_lhs_type)->p_value_type;
    } else if (p_rhs_type->kind == KETL_TYPE_ARRAY) {
        if (((ketl_type_array*)p_rhs_type)->p_value_type != p_lhs_type) {
            errorf(expr_info.source_offset, expr_info.length, "Can't concat array with different value type.");
            return push_temp_var(p_context, expr_info);
        }
        
        p_result_value_type = ((ketl_type_array*)p_rhs_type)->p_value_type;
    } else {
        if (p_lhs_type != p_rhs_type) {
            errorf(expr_info.source_offset, expr_info.length, "Can't concat array with different value type.");
            return push_temp_var(p_context, expr_info);
        }
        
        p_result_value_type = p_lhs_type;
    }

    ketl_hir_used_type_index_t size_type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, ketl_state_get_u64(p_context->p_state));        
    ketl_hir_var_id_t zero_var = ketl_hir_builder_get_literal(&p_context->hir_builder, 
        (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_context->hir_builder.symbols, "0", 1), expr_info, size_type);

    ketl_hir_var_id_t output_var = push_hir_new_array(p_context, ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_result_value_type), zero_var, expr_info);

    output_var = push_append(p_context, output_var, lhs);
    output_var = push_append(p_context, output_var, rhs);

    return output_var;
}

static ketl_hir_var_id_t parse_binary_ltr(ketl_parser_context* p_context, ketl_hir_var_id_t lhs) {
    ketl_token_type token_type = CURRENT_TOKEN(1).type;
    ketl_parse_rule* p_parse_rule = get_parse_rule(token_type);
    ketl_hir_var_id_t rhs = parse_precedence(p_context, p_parse_rule->precedence + 1);

    ANN_SWITCH_STRICT (token_type) {
        case KETL_TOKEN_TYPE_PLUS:                return push_hir_binary_op(p_context, KETL_HIR_PLUS,                lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_MINUS:               return push_hir_binary_op(p_context, KETL_HIR_MINUS,               lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_MULTIPLY:            return push_hir_binary_op(p_context, KETL_HIR_MULTY,               lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_DIVIDE:              return push_hir_binary_op(p_context, KETL_HIR_DIV,                 lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_REMAINDER:           return push_hir_binary_op(p_context, KETL_HIR_MOD,                 lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
       
        case KETL_TOKEN_TYPE_CONCAT:              return push_hir_concat   (p_context,                               lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));

        case KETL_TOKEN_TYPE_LESS:                return push_hir_binary_op(p_context, KETL_HIR_LESS,                lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_LESS_OR_EQUAL:       return push_hir_binary_op(p_context, KETL_HIR_LESS_OR_EQUAL,       lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_GREATER:             return push_hir_binary_op(p_context, KETL_HIR_GREATER,             lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_GREATER_OR_EQUAL:    return push_hir_binary_op(p_context, KETL_HIR_GREATER_OR_EQUAL,    lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_EQUAL:               return push_hir_binary_op(p_context, KETL_HIR_EQUAL,               lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_NOT_EQUAL:           return push_hir_binary_op(p_context, KETL_HIR_NOT_EQUAL,           lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));

        case KETL_TOKEN_TYPE_BITWISE_AND:         return push_hir_binary_op(p_context, KETL_HIR_BITWISE_AND,         lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_BITWISE_OR:          return push_hir_binary_op(p_context, KETL_HIR_BITWISE_OR,          lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_BITWISE_XOR:         return push_hir_binary_op(p_context, KETL_HIR_BITWISE_XOR,         lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));

        case KETL_TOKEN_TYPE_BITWISE_SHIFT_LEFT:  return push_hir_binary_op(p_context, KETL_HIR_BITWISE_SHIFT_LEFT,  lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
        case KETL_TOKEN_TYPE_BITWISE_SHIFT_RIGHT: return push_hir_binary_op(p_context, KETL_HIR_BITWISE_SHIFT_RIGHT, lhs, rhs, expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info));
    }
}

static ketl_hir_var_id_t parse_short_circuit(ketl_parser_context* p_context, ketl_hir_var_id_t lhs) {
    ketl_token_type token_type = CURRENT_TOKEN(1).type;
    ketl_parse_rule* p_parse_rule = get_parse_rule(token_type);

    ketl_hir_block_index_t first_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 3);
    ketl_hir_block_index_t true_statement = first_block;
    ketl_hir_block_index_t false_statement = first_block + 1;
    ketl_hir_block_index_t after_block = first_block + 2;

    ketl_hir_builder_push_if(&p_context->hir_builder, lhs, true_statement, false_statement);

    ketl_hir_block_index_t short_sircuit_block, second_block;
    ANN_SWITCH_STRICT (token_type) {
        case KETL_TOKEN_TYPE_LOGICAL_OR: {
            short_sircuit_block = true_statement;
            second_block = false_statement;
            break;
        }
        case KETL_TOKEN_TYPE_LOGICAL_AND: {
            short_sircuit_block = false_statement;
            second_block = true_statement;
            break;
        }
    }

    ketl_hir_builder_set_block(&p_context->hir_builder, second_block);
    ketl_hir_var_id_t rhs = parse_precedence(p_context, p_parse_rule->precedence + 1);

    ketl_hir_expr_info_t expr_info = expr_info_merge(GET_VAR(lhs).expr_info, GET_VAR(rhs).expr_info);
    
    // TODO find common type, set temp var to common type
    if (GET_VAR(lhs).type != GET_VAR(rhs).type) {
        errorf(expr_info.source_offset, expr_info.length, "Can't compare two types.");
        return push_temp_var(p_context, expr_info);
    }
    if (GET_VAR(lhs).type == KETL_HIR_USED_TYPE_UNKNOWN || GET_VAR(rhs).type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return push_temp_var(p_context, expr_info);
    }
    ketl_hir_var_id_t output_var = push_temp_var_type(p_context, expr_info, GET_VAR(lhs).type); 
    // TODO do casting if necessary
    push_hir_assign_impl(p_context, KETL_HIR_ASSIGN, output_var, rhs);
    ketl_hir_builder_push_jump(&p_context->hir_builder, after_block);
    
    ketl_hir_builder_set_block(&p_context->hir_builder, short_sircuit_block);
    // TODO do casting if necessary
    push_hir_assign_impl(p_context, KETL_HIR_ASSIGN, output_var, lhs);
    ketl_hir_builder_push_jump(&p_context->hir_builder, after_block);

    ketl_hir_builder_set_block(&p_context->hir_builder, after_block);

    return output_var;
}

static ketl_hir_var_id_t parse_binary_rtl(ketl_parser_context* p_context, ketl_hir_var_id_t lhs, ketl_hir_var_id_t rhs) {
    ketl_token_type token_type = CURRENT_TOKEN(1).type;

    ANN_SWITCH_STRICT (token_type) {
        case KETL_TOKEN_TYPE_ASSIGN            : return push_hir_assign(p_context, KETL_HIR_ASSIGN, lhs, rhs);

        case KETL_TOKEN_TYPE_ASSIGN_PLUS       : return push_hir_assign(p_context, KETL_HIR_ASSIGN_PLUS, lhs, rhs);
        case KETL_TOKEN_TYPE_ASSIGN_MINUS      : return push_hir_assign(p_context, KETL_HIR_ASSIGN_MINUS, lhs, rhs);
        case KETL_TOKEN_TYPE_ASSIGN_MULTIPLY   : return push_hir_assign(p_context, KETL_HIR_ASSIGN_MULTY, lhs, rhs);
        case KETL_TOKEN_TYPE_ASSIGN_DIVIDE     : return push_hir_assign(p_context, KETL_HIR_ASSIGN_DIV, lhs, rhs);
        case KETL_TOKEN_TYPE_ASSIGN_REMAINDER  : return push_hir_assign(p_context, KETL_HIR_ASSIGN_MOD, lhs, rhs);

        case KETL_TOKEN_TYPE_ASSIGN_BITWISE_AND: return push_hir_assign(p_context, KETL_HIR_ASSIGN_BITWISE_AND, lhs, rhs);
        case KETL_TOKEN_TYPE_ASSIGN_BITWISE_OR : return push_hir_assign(p_context, KETL_HIR_ASSIGN_BITWISE_OR, lhs, rhs);
        case KETL_TOKEN_TYPE_ASSIGN_BITWISE_XOR: return push_hir_assign(p_context, KETL_HIR_ASSIGN_BITWISE_XOR, lhs, rhs);

        case KETL_TOKEN_TYPE_ASSIGN_CONCAT     : return push_append    (p_context, lhs, rhs);
    }
}

ketl_parse_rule parse_rules[] = {
    [KETL_TOKEN_TYPE_ID]                         = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_LITERAL_NULL]               = { parse_null,          NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_LITERAL_FALSE]              = { parse_false,         NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_LITERAL_TRUE]               = { parse_true,          NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_LITERAL_INTEGER]            = { parse_number,        NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_LITERAL_STRING]             = { parse_string,        NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_LITERAL_CHAR]               = { parse_char,          NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_PARENTHESIS_LEFT]           = { parse_grouping,      parse_call,            NULL,             KETL_PREC_CALL},
    [KETL_TOKEN_TYPE_PARENTHESIS_RIGHT]          = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CURLY_LEFT]                 = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CURLY_RIGHT]                = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_SQUARE_LEFT]                = { NULL,                parse_indexing,        NULL,             KETL_PREC_CALL},
    [KETL_TOKEN_TYPE_SQUARE_RIGHT]               = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DOT]                        = { NULL,                parse_dot_operator,    NULL,             KETL_PREC_CALL},
    [KETL_TOKEN_TYPE_COMMA]                      = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_QUESTION_MARK]              = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DOLLAR]                     = { parse_dollar_prefix, parse_dollar_operator, NULL,             KETL_PREC_CALL},
    [KETL_TOKEN_TYPE_COLON]                      = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_AT]                         = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DOUBLE_ARROW_RIGHT]         = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ARROW_RIGHT]                = { NULL,                parse_arrow_operator,  NULL,             KETL_PREC_CALL},
    [KETL_TOKEN_TYPE_TERMINATION_CHARACTER]      = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_LOGICAL_NOT]                = { parse_unary_rtl,     NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_LOGICAL_AND]                = { NULL,                parse_short_circuit,   NULL,             KETL_PREC_LOGICAL_AND},
    [KETL_TOKEN_TYPE_LOGICAL_OR]                 = { NULL,                parse_short_circuit,   NULL,             KETL_PREC_LOGICAL_OR},
    [KETL_TOKEN_TYPE_LESS]                       = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_COMPARISON},
    [KETL_TOKEN_TYPE_LESS_OR_EQUAL]              = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_COMPARISON},
    [KETL_TOKEN_TYPE_GREATER]                    = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_COMPARISON},
    [KETL_TOKEN_TYPE_GREATER_OR_EQUAL]           = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_COMPARISON},
    [KETL_TOKEN_TYPE_EQUAL]                      = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_EQUALITY},
    [KETL_TOKEN_TYPE_NOT_EQUAL]                  = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_EQUALITY},
    [KETL_TOKEN_TYPE_BITWISE_NOT]                = { parse_unary_rtl,     NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_BITWISE_AND]                = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_BITWISE_AND},
    [KETL_TOKEN_TYPE_BITWISE_OR]                 = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_BITWISE_OR},
    [KETL_TOKEN_TYPE_BITWISE_XOR]                = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_BITWISE_XOR},
    [KETL_TOKEN_TYPE_BITWISE_SHIFT_LEFT]         = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_SHIFT},
    [KETL_TOKEN_TYPE_BITWISE_SHIFT_RIGHT]        = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_SHIFT},
    [KETL_TOKEN_TYPE_INCREMENT]                  = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DECREMENT]                  = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_PLUS]                       = { parse_unary_rtl,     parse_binary_ltr,      NULL,             KETL_PREC_TERM},
    [KETL_TOKEN_TYPE_MINUS]                      = { parse_unary_rtl,     parse_binary_ltr,      NULL,             KETL_PREC_TERM},
    [KETL_TOKEN_TYPE_MULTIPLY]                   = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_FACTOR},
    [KETL_TOKEN_TYPE_DIVIDE]                     = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_FACTOR},
    [KETL_TOKEN_TYPE_REMAINDER]                  = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_FACTOR},
    [KETL_TOKEN_TYPE_CONCAT]                     = { NULL,                parse_binary_ltr,      NULL,             KETL_PREC_CONCAT},
    [KETL_TOKEN_TYPE_ASSIGN]                     = { NULL,                NULL,                  parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_ASSIGN_PLUS]                = { NULL,                NULL,                  parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_ASSIGN_MINUS]               = { NULL,                NULL,                  parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_ASSIGN_MULTIPLY]            = { NULL,                NULL,                  parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_ASSIGN_DIVIDE]              = { NULL,                NULL,                  parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_ASSIGN_REMAINDER]           = { NULL,                NULL,                  parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_ASSIGN_CONCAT]              = { NULL,                NULL,                  parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_ASSIGN_BITWISE_SHIFT_LEFT]  = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_BITWISE_SHIFT_RIGHT] = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_BITWISE_AND]         = { NULL,                NULL,                  parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_ASSIGN_BITWISE_OR]          = { NULL,                NULL,                  parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_ASSIGN_BITWISE_XOR]         = { NULL,                NULL,                  parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_I8]                         = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_I16]                        = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_I32]                        = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_I64]                        = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_U8]                         = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_U16]                        = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_U32]                        = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_U64]                        = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_F32]                        = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_F64]                        = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_BOOL]                       = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_BREAK]                      = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CASE]                       = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CEXPORT]                    = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CHAR]                       = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_CIMPORT]                    = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CONST]                      = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CONTINUE]                   = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DEFAULT]                    = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DO]                         = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ELSE]                       = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ENUM]                       = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_FN]                         = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_FOR]                        = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_FROM]                       = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_IF]                         = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_IMPORT]                     = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_MIMIC]                      = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_NONE]                       = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_RAW]                        = { parse_identificator, NULL,                  NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_RETURN]                     = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_STRUCT]                     = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_SWITCH]                     = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_UNION]                      = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_VAR]                        = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_WHILE]                      = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_EOF]                        = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ERROR]                      = { NULL,                NULL,                  NULL,             KETL_PREC_NONE},
};

static ketl_parse_rule* get_parse_rule(ketl_token_type token_type) {
    return &parse_rules[token_type];
}

static ketl_hir_var_id_t parse_lhs_operand(ketl_parser_context* p_context, ketl_precedence precedence) {    
    ketl_parse_unary_rtl f_prefix_rule = get_parse_rule(CURRENT_TOKEN(1).type)->f_prefix;
    if (f_prefix_rule == NULL) {
        errorf(CURRENT_TOKEN(1).offset, CURRENT_TOKEN(1).length, "Expected expression.");
        return push_temp_var(p_context, token_extract_info(CURRENT_TOKEN(1)));
    }

    ketl_hir_var_id_t lhs = f_prefix_rule(p_context);

    while (precedence < get_parse_rule(CURRENT_TOKEN(0).type)->precedence ||
        (associativity[precedence] == KETL_LTR && precedence == get_parse_rule(CURRENT_TOKEN(0).type)->precedence)) {
        token_advance(p_context);
        ketl_parse_ltr_infix f_ltr_infix_rule = get_parse_rule(CURRENT_TOKEN(1).type)->f_ltr_infix;
        if (f_ltr_infix_rule == NULL) {
            errorf(CURRENT_TOKEN(1).offset, CURRENT_TOKEN(1).length, "Expected operator.");
            return push_temp_var(p_context, token_extract_info(CURRENT_TOKEN(1)));
        }
        lhs = f_ltr_infix_rule(p_context, lhs);
    }
   
    return lhs;
}

// returns true if closing bracket met
static bool dummy_parse_bracket(ketl_token_type token_type, ketl_parser_bracket_t* p_brackets_stack, uint32_t* bracket_stack_count) {
    ketl_parser_bracket_t bracket;
    switch (token_type) {
        case KETL_TOKEN_TYPE_PARENTHESIS_LEFT:
            p_brackets_stack[(*bracket_stack_count)++] = KETL_PARSER_BRACKET_PARENTHESIS;
            return false;
        case KETL_TOKEN_TYPE_CURLY_LEFT:
            p_brackets_stack[(*bracket_stack_count)++] = KETL_PARSER_BRACKET_CURLY;
            return false;
        case KETL_TOKEN_TYPE_SQUARE_LEFT:
            p_brackets_stack[(*bracket_stack_count)++] = KETL_PARSER_BRACKET_SQUARE;
            return false;
        case KETL_TOKEN_TYPE_PARENTHESIS_RIGHT:
            if (*bracket_stack_count <= 0) {
                return false;
            }
            bracket = p_brackets_stack[--(*bracket_stack_count)];
            return (bracket == KETL_PARSER_BRACKET_PARENTHESIS);
        case KETL_TOKEN_TYPE_CURLY_RIGHT:
            if (*bracket_stack_count <= 0) {
                return false;
            }
            bracket = p_brackets_stack[--(*bracket_stack_count)];
            return (bracket == KETL_PARSER_BRACKET_CURLY);
        case KETL_TOKEN_TYPE_SQUARE_RIGHT:
            if (*bracket_stack_count <= 0) {
                return false;
            }
            bracket = p_brackets_stack[--(*bracket_stack_count)];
            return (bracket == KETL_PARSER_BRACKET_SQUARE);
    }
    return false;
}

static ketl_hir_var_id_t parse_precedence(ketl_parser_context* p_context, ketl_precedence precedence) {
    token_advance(p_context);

    if (associativity[precedence] == KETL_LTR) {
        ketl_hir_var_id_t lhs = parse_lhs_operand(p_context, precedence);
        return lhs;
    }

    // looking for the rtl operator
    ketl_token_iterator_t start_pos = p_context->p_lexer->token_iterator;
    ketl_parser_bracket_t a_brackets_stack[32];
    uint32_t bracket_stack_count = 0;

    dummy_parse_bracket(CURRENT_TOKEN(1).type, a_brackets_stack, &bracket_stack_count);
    ketl_parse_rule* p_parse_rule = get_parse_rule(CURRENT_TOKEN(1).type);
    ketl_precedence token_precedence = p_parse_rule->precedence;
    while (precedence < token_precedence) {
        token_advance(p_context);

        ketl_token_t token = CURRENT_TOKEN(1);
        ketl_token_type current_token_type = token.type;
        if (current_token_type == KETL_TOKEN_TYPE_EOF) {
            errorf(token.offset, token.length, "Expected operator.");
            return push_temp_var(p_context, token_extract_info(token));
        }

        p_parse_rule = get_parse_rule(current_token_type);
        if (!dummy_parse_bracket(current_token_type, a_brackets_stack, &bracket_stack_count)) {
            token_precedence = p_parse_rule->precedence;
        }
    }

    // we coudln't find rtl operator, restoring token iterator and do simple ltr
    if (precedence != token_precedence) {
        p_context->p_lexer->token_iterator = start_pos;
        
        ketl_hir_var_id_t lhs = parse_lhs_operand(p_context, precedence);
        return lhs;
    }

    // parse after the operator, rhs operand
    ketl_hir_var_id_t rhs = parse_precedence(p_context, precedence);

    // saving end of the rhs operand for restoring later
    ketl_token_iterator_t end_pos = p_context->p_lexer->token_iterator;
    p_context->p_lexer->token_iterator = start_pos;

    ketl_hir_var_id_t lhs = parse_lhs_operand(p_context, precedence);
        
    token_advance(p_context);

    // back to the rtl operator
    ketl_parse_rtl_infix f_rtl_infix = get_parse_rule(CURRENT_TOKEN(1).type)->f_rtl_infix;
    if (f_rtl_infix == NULL) {
        errorf(CURRENT_TOKEN(2).offset, CURRENT_TOKEN(2).length, "Expected operator.");
        return push_temp_var(p_context, token_extract_info(CURRENT_TOKEN(2)));
    }
    lhs = f_rtl_infix(p_context, lhs, rhs);

    // restore token iterator
    p_context->p_lexer->token_iterator = end_pos;
    return lhs;
}

static ketl_hir_var_id_t parse_expression(ketl_parser_context* p_context) {
    return parse_precedence(p_context, KETL_PREC_ASSIGNMENT);
}

static ketl_statement_info parse_expression_statement(ketl_parser_context* p_context) {
    parse_expression(p_context);
    token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' after expression.");
    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_block_statement_inner(ketl_parser_context* p_context) {
    ketl_parse_return_info return_info = KETL_RETURN_EMPTY;
    while (!token_check(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT) && !token_check(p_context, KETL_TOKEN_TYPE_EOF)) {
        if (return_info & KETL_RETURN_ALWAYS) {
            uint32_t statement_start = CURRENT_TOKEN(0).offset;
            ketl_parse_return_info decl_return_info = parse_declaration(p_context).return_info;
            uint32_t statement_end = CURRENT_TOKEN(1).offset + CURRENT_TOKEN(1).length;

            errorf(statement_start, statement_end - statement_start, "Unreachable statement.");

            return_info |= decl_return_info;
        } else {
            return_info |= parse_declaration(p_context).return_info;
        }
    }
    return (ketl_statement_info){ .return_info = return_info };
}

static void push_symbol_map(ketl_parser_context* p_context) {
    hir_builder_symbol_stack_t_push_back_copy(&p_context->hir_builder.symbol_to_var_info_stack, (hir_builder_symbol_to_var_info_map_t){0});
    hir_builder_symbol_to_var_info_map_t_init(
        &p_context->hir_builder.symbol_to_var_info_stack.p_data[p_context->hir_builder.symbol_to_var_info_stack.size - 1],
        p_context->hir_builder.p_allocator);
}

static void pop_symbol_map(ketl_parser_context* p_context) {
    hir_builder_symbol_to_var_info_map_t_deinit(
        &p_context->hir_builder.symbol_to_var_info_stack.p_data[p_context->hir_builder.symbol_to_var_info_stack.size - 1]);
    --p_context->hir_builder.symbol_to_var_info_stack.size;
}

static ketl_statement_info parse_block_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // {

    push_symbol_map(p_context);
    ketl_statement_info statement_info = parse_block_statement_inner(p_context);
    pop_symbol_map(p_context);

    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT, "Expected '}' at the end of the block.");
    return statement_info;
}

static ketl_statement_info parse_if_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // if

    ketl_hir_block_index_t first_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 2);
    ketl_hir_block_index_t true_statement = first_block;
    ketl_hir_block_index_t false_statement = first_block + 1;

    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, "Expected '(' after 'if' keyword.");
    ketl_hir_var_id_t expr = parse_expression(p_context);
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after 'if' statement expression.");

    ketl_hir_builder_push_if(&p_context->hir_builder, expr, true_statement, false_statement);

    ketl_hir_builder_set_block(&p_context->hir_builder, true_statement);
    ketl_parse_return_info true_return_info = parse_statement(p_context).return_info;

    ketl_parse_return_info return_info;

    if (token_match(p_context, KETL_TOKEN_TYPE_ELSE)) {
        ketl_parse_return_info false_return_info;
        if (true_return_info & KETL_RETURN_ALWAYS) {
            ketl_hir_builder_set_block(&p_context->hir_builder, false_statement);
            false_return_info = parse_statement(p_context).return_info;
            if (!(false_return_info & KETL_RETURN_ALWAYS)) {
                ketl_hir_block_index_t after_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 1);
                ketl_hir_builder_push_jump(&p_context->hir_builder, after_block);
                ketl_hir_builder_set_block(&p_context->hir_builder, after_block);
            }
        } else {
            ketl_hir_block_index_t after_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 1);
            ketl_hir_builder_push_jump(&p_context->hir_builder, after_block);
        
            ketl_hir_builder_set_block(&p_context->hir_builder, false_statement);
            false_return_info = parse_statement(p_context).return_info;
            if (!(false_return_info & KETL_RETURN_ALWAYS)) {
                ketl_hir_builder_push_jump(&p_context->hir_builder, after_block);
            }

            ketl_hir_builder_set_block(&p_context->hir_builder, after_block);
        }

        return_info = 
            ((true_return_info & false_return_info) & KETL_RETURN_ALWAYS) |
            ((true_return_info | false_return_info) & KETL_RETURN_UNDEF);
    } else {
        if (!(true_return_info & KETL_RETURN_ALWAYS)) {
            ketl_hir_builder_push_jump(&p_context->hir_builder, false_statement);
        }
        ketl_hir_builder_set_block(&p_context->hir_builder, false_statement);

        return_info = true_return_info & KETL_RETURN_UNDEF;
    }

    return (ketl_statement_info){ .return_info = return_info };
}

static ketl_statement_info parse_break_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // break

    ketl_statement_info statement_info = { .return_info = KETL_RETURN_EMPTY };

    if (p_context->reserved_break == (ketl_hir_block_index_t)-1) {
        ketl_token_t literal = CURRENT_TOKEN(1);
        errorf(literal.offset, literal.length, "'break' statement outside of loop or switch body.");
    } else {
        ketl_hir_builder_push_jump(&p_context->hir_builder, p_context->reserved_break);
    }
        
    token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' after 'break' statement.");
    return statement_info;
}

static ketl_statement_info parse_continue_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // continue

    ketl_statement_info statement_info = { .return_info = KETL_RETURN_EMPTY };

    if (p_context->reserved_continue == (ketl_hir_block_index_t)-1) {
        ketl_token_t literal = CURRENT_TOKEN(1);
        errorf(literal.offset, literal.length, "'continue' outside of loop body.");
    } else {
        ketl_hir_builder_push_jump(&p_context->hir_builder, p_context->reserved_continue);
    }
        
    token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' after 'continue' statement.");
    return statement_info;
}

static ketl_statement_info parse_while_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // while

    ketl_hir_block_index_t first_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 3);
    ketl_hir_block_index_t expr_statement = first_block;
    ketl_hir_block_index_t body_statement = first_block + 1;
    ketl_hir_block_index_t after_statement = first_block + 2;

    ketl_hir_builder_push_jump(&p_context->hir_builder, expr_statement);
    ketl_hir_builder_set_block(&p_context->hir_builder, expr_statement);
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, "Expected '(' after 'while' keyword.");
    ketl_hir_var_id_t expr = parse_expression(p_context);
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after 'while' statement expression.");

    ketl_hir_builder_push_if(&p_context->hir_builder, expr, body_statement, after_statement);

    ketl_hir_block_index_t old_reserved_break = p_context->reserved_break;
    p_context->reserved_break = after_statement;
    ketl_hir_block_index_t old_reserved_continue = p_context->reserved_continue;
    p_context->reserved_continue = expr_statement;

    ketl_hir_builder_set_block(&p_context->hir_builder, body_statement);
    ketl_parse_return_info body_return_info = parse_statement(p_context).return_info;

    p_context->reserved_continue = old_reserved_continue;
    p_context->reserved_break = old_reserved_break;

    if (!(body_return_info & KETL_RETURN_ALWAYS)) {
        ketl_hir_builder_push_jump(&p_context->hir_builder, expr_statement);
    }
    ketl_hir_builder_set_block(&p_context->hir_builder, after_statement);

    ketl_parse_return_info return_info = body_return_info & KETL_RETURN_UNDEF;

    return (ketl_statement_info){ .return_info = return_info };
}

static ketl_hir_var_id_t push_default_initial_value(ketl_parser_context* p_context, ketl_type* p_type, ketl_hir_expr_info_t expr_info) {
    // TODO do default contructor or smth
    if (p_type->kind == KETL_TYPE_PRIMITIVE) {
        return push_literal_symbol_of_type(p_context, KETL_HIR_LITERAL_NULL, expr_info, ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_type));
    } else {
        return push_null_var(p_context, expr_info);
    }
}

static ketl_statement_info parse_for_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // for

    ketl_hir_expr_info_t expr_info = token_extract_info(CURRENT_TOKEN(1));

    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, "Expected '(' after 'for' keyword.");
    token_consume(p_context, KETL_TOKEN_TYPE_VAR, "Expected 'var' keyword in the beggining of 'for' statement expression.");
    ketl_token_t id_literal = CURRENT_TOKEN(0);
    token_consume(p_context, KETL_TOKEN_TYPE_ID, "Expected id after 'var' keyword.");

    ketl_hir_used_type_index_t var_type = KETL_HIR_USED_TYPE_UNKNOWN;
    if (token_match(p_context, KETL_TOKEN_TYPE_COLON)) {
        var_type = parse_type_as_index(p_context);
    }

    token_consume(p_context, KETL_TOKEN_TYPE_ARROW_LEFT, "Expected '<-' after 'var' declaration.");
    ketl_hir_var_id_t array_var = parse_expression(p_context);

    if (GET_VAR(array_var).type >= KETL_HIR_USED_TYPE_LAST) {
        return (ketl_statement_info){ .return_info = KETL_RETURN_NONE };
    }

    ketl_type* p_array_type = GET_TYPE(GET_VAR(array_var).type);
    ANN_ASSERT(p_array_type->kind == KETL_TYPE_ARRAY);

    if (var_type == KETL_HIR_USED_TYPE_UNKNOWN) {
        var_type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, ((ketl_type_array*)p_array_type)->p_value_type); 
    }

    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after 'for' statement expression.");

    expr_info = expr_info_merge(expr_info, token_extract_info(CURRENT_TOKEN(1)));

    push_symbol_map(p_context);

    ketl_hir_used_type_index_t size_type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, ketl_state_get_u64(p_context->p_state));
    ketl_hir_symbol_offset_t size_symbol = (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_context->hir_builder.symbols, "size", 4);

    ketl_hir_var_id_t iterator_var = push_hir_variable_declaration(p_context, id_literal, var_type, 
        push_default_initial_value(p_context, GET_TYPE(var_type), expr_info), expr_info);
    ketl_hir_var_id_t index_var = ketl_hir_builder_create_temp_var(&p_context->hir_builder, expr_info, size_type);
    ketl_hir_var_id_t zero_var = ketl_hir_builder_get_literal(&p_context->hir_builder, KETL_HIR_LITERAL_NULL, expr_info, size_type);
    ketl_hir_builder_push_assign(&p_context->hir_builder, KETL_HIR_ASSIGN, index_var, zero_var);

    ketl_hir_var_id_t array_size_var = ketl_hir_builder_create_field_var(&p_context->hir_builder, array_var, size_symbol, expr_info, size_type);

    ketl_hir_block_index_t first_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 4);
    ketl_hir_block_index_t expr_statement = first_block;
    ketl_hir_block_index_t body_statement = first_block + 1;
    ketl_hir_block_index_t inc_statement = first_block + 2;
    ketl_hir_block_index_t after_statement = first_block + 3;

    ketl_hir_builder_push_jump(&p_context->hir_builder, expr_statement);

    ketl_hir_builder_set_block(&p_context->hir_builder, expr_statement);
    ketl_hir_var_id_t is_index_less_size_var = ketl_hir_builder_push_hir_cmp_op(&p_context->hir_builder, KETL_HIR_LESS, index_var, array_size_var, expr_info);
    ketl_hir_builder_push_if(&p_context->hir_builder, is_index_less_size_var, body_statement, after_statement);

    ketl_hir_builder_set_block(&p_context->hir_builder, body_statement);
    ketl_hir_var_id_t array_indexed_var = ketl_hir_builder_create_index_var(&p_context->hir_builder, array_var, index_var, expr_info);
    array_indexed_var = trying_to_cast_rhs_to_lhs(p_context, GET_VAR(iterator_var).type, GET_VAR(iterator_var).expr_info, array_indexed_var, false);
    ketl_hir_builder_push_assign(&p_context->hir_builder, KETL_HIR_ASSIGN, iterator_var, array_indexed_var);

    ketl_hir_block_index_t old_reserved_break = p_context->reserved_break;
    p_context->reserved_break = after_statement;
    ketl_hir_block_index_t old_reserved_continue = p_context->reserved_continue;
    p_context->reserved_continue = inc_statement;

    ketl_parse_return_info body_return_info = parse_statement(p_context).return_info;
    ketl_hir_builder_push_jump(&p_context->hir_builder, inc_statement);

    ketl_hir_builder_set_block(&p_context->hir_builder, inc_statement);
    ketl_hir_var_id_t one_var = ketl_hir_builder_get_literal(&p_context->hir_builder, 
        (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_context->hir_builder.symbols, "1", 1), expr_info, size_type);
    ketl_hir_builder_push_assign(&p_context->hir_builder, KETL_HIR_ASSIGN_PLUS, index_var, one_var);
    
    p_context->reserved_break = old_reserved_break;
    p_context->reserved_continue = old_reserved_continue;

    if (!(body_return_info & KETL_RETURN_ALWAYS)) {
        ketl_hir_builder_push_jump(&p_context->hir_builder, expr_statement);
    }
    ketl_hir_builder_set_block(&p_context->hir_builder, after_statement);

    pop_symbol_map(p_context);

    ketl_parse_return_info return_info = body_return_info & KETL_RETURN_UNDEF;

    return (ketl_statement_info){ .return_info = return_info };
}

static ketl_statement_info parse_switch_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // switch

    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, "Expected '(' after 'switch' keyword.");
    ketl_hir_var_id_t expr = parse_expression(p_context);
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after 'switch' statement expression.");

    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_LEFT, "Expected '{' after 'switch' statement.");
    
    ketl_parse_return_info return_info = KETL_RETURN_ALWAYS;
    ketl_parse_return_info default_return_info = KETL_RETURN_UNDEF;

    ketl_hir_block_index_t default_block = (ketl_hir_block_index_t)-1;

    if (!token_match(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT)) {
        ketl_hir_block_index_t first_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 2);
        ketl_hir_block_index_t after_statement = first_block;
        ketl_hir_block_index_t next_cmp_block = first_block + 1;
        ketl_hir_builder_push_jump(&p_context->hir_builder, next_cmp_block);

        ketl_hir_block_index_t old_reserved_break = p_context->reserved_break;
        p_context->reserved_break = after_statement;

        while (!token_match(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT)) {
            ketl_hir_expr_info_t expr_info = token_extract_info(CURRENT_TOKEN(0));

            switch(CURRENT_TOKEN(0).type) {
                case KETL_TOKEN_TYPE_CASE: {
                    token_advance(p_context); // case

                    ketl_hir_block_index_t current_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 1);

                    ketl_hir_builder_set_block(&p_context->hir_builder, next_cmp_block);
                    ketl_hir_var_id_t case_expr = parse_expression(p_context);

                    while (token_match(p_context, KETL_TOKEN_TYPE_CASE)) {
                        expr_info = GET_VAR(case_expr).expr_info;
                        ketl_hir_var_id_t cmp_expr = push_hir_binary_op(p_context, KETL_HIR_EQUAL, expr, case_expr, expr_info);

                        next_cmp_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 1);
                        
                        ketl_hir_builder_push_if(&p_context->hir_builder, cmp_expr, current_block, next_cmp_block);

                        ////////////////

                        ketl_hir_builder_set_block(&p_context->hir_builder, next_cmp_block);
                        case_expr = parse_expression(p_context);
                    }

                    token_consume(p_context, KETL_TOKEN_TYPE_DOUBLE_ARROW_RIGHT, "Expected '=>' after 'case' statement expression.");

                    expr_info = GET_VAR(case_expr).expr_info;
                    ketl_hir_var_id_t cmp_expr = push_hir_binary_op(p_context, KETL_HIR_EQUAL, expr, case_expr, expr_info);

                    next_cmp_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 1);
                    
                    ketl_hir_builder_push_if(&p_context->hir_builder, cmp_expr, current_block, next_cmp_block);

                    ///////////

                    ketl_hir_builder_set_block(&p_context->hir_builder, current_block);
                    ketl_parse_return_info body_return_info = parse_statement(p_context).return_info;

                    if (!(body_return_info & KETL_RETURN_ALWAYS)) {
                        ketl_hir_builder_push_jump(&p_context->hir_builder, after_statement);
                    }

                    return_info = 
                        ((return_info & body_return_info) & KETL_RETURN_ALWAYS) |
                        ((return_info | body_return_info) & KETL_RETURN_UNDEF);
                    break;
                }
                case KETL_TOKEN_TYPE_DEFAULT: {
                    token_advance(p_context); // default

                    token_consume(p_context, KETL_TOKEN_TYPE_DOUBLE_ARROW_RIGHT, "Expected '=>' after 'default' statement expression.");

                    ANN_ASSERT(default_block == (ketl_hir_block_index_t)-1);
                    default_block = ketl_hir_builder_reserve_blocks(&p_context->hir_builder, 1);
                    ketl_hir_builder_set_block(&p_context->hir_builder, default_block);

                    ketl_parse_return_info body_return_info = parse_statement(p_context).return_info;

                    if (!(body_return_info & KETL_RETURN_ALWAYS)) {
                        ketl_hir_builder_push_jump(&p_context->hir_builder, after_statement);
                    }

                    default_return_info = body_return_info;
                    break;
                }
                default: {
                    errorf(expr_info.source_offset, expr_info.length, "Unexpected token inside 'switch' body.");
                    token_advance(p_context);
                }
            }
        }

        p_context->reserved_break = old_reserved_break;

        ketl_hir_builder_set_block(&p_context->hir_builder, next_cmp_block);

        if (default_block == (ketl_hir_block_index_t)-1) {
            ketl_hir_builder_push_jump(&p_context->hir_builder, after_statement);

            return_info &= KETL_RETURN_UNDEF;
        } else {
            ketl_hir_builder_push_jump(&p_context->hir_builder, default_block);

            return_info = 
                ((return_info & default_return_info) & KETL_RETURN_ALWAYS) |
                ((return_info | default_return_info) & KETL_RETURN_UNDEF);
        }
            
        if (!(return_info & KETL_RETURN_ALWAYS)) {
            ketl_hir_builder_set_block(&p_context->hir_builder, after_statement);
        }
    } else {
        return_info &= KETL_RETURN_UNDEF;
    }

    return (ketl_statement_info){ .return_info = return_info };
}

static ketl_statement_info parse_return_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // return
    if (token_match(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER)) {
        push_hir_instr(p_context, KETL_HIR_RETURN);
        return (ketl_statement_info){ .return_info = KETL_RETURN_ALWAYS_NONE };
    } else {
        push_hir_return_value(p_context, parse_expression(p_context), token_extract_info(CURRENT_TOKEN(1)));
        token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' after return statement expression.");
        return (ketl_statement_info){ .return_info = KETL_RETURN_ALWAYS_VALUE };
    }
}

static ketl_statement_info parse_statement(ketl_parser_context* p_context) {
    switch (CURRENT_TOKEN(0).type) {
        case KETL_TOKEN_TYPE_CURLY_LEFT           : return parse_block_statement     (p_context); break;
        case KETL_TOKEN_TYPE_IF                   : return parse_if_statement        (p_context); break;
        case KETL_TOKEN_TYPE_WHILE                : return parse_while_statement     (p_context); break;
        case KETL_TOKEN_TYPE_FOR                  : return parse_for_statement       (p_context); break;
        case KETL_TOKEN_TYPE_SWITCH               : return parse_switch_statement    (p_context); break;
        case KETL_TOKEN_TYPE_BREAK                : return parse_break_statement     (p_context); break;
        case KETL_TOKEN_TYPE_CONTINUE             : return parse_continue_statement  (p_context); break;
        case KETL_TOKEN_TYPE_RETURN               : return parse_return_statement    (p_context); break;
        case KETL_TOKEN_TYPE_TERMINATION_CHARACTER:        token_advance             (p_context); 
                                                    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
        default                                   : return parse_expression_statement(p_context); break;
    }
}

static ketl_statement_info parse_enum_declaration(ketl_parser_context* p_context) {
    token_advance(p_context); // enum
    ketl_token_t id_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // id

    ketl_type_primitive* p_parent_primitive = (ketl_type_primitive*) ketl_state_get_u8(p_context->p_state);
    bool defined_type = token_match(p_context, KETL_TOKEN_TYPE_COLON);
    if (defined_type) {
        ketl_type* p_type = parse_type(p_context);
        if (!p_type || p_type->kind != KETL_TYPE_PRIMITIVE) {
            errorf(id_literal.offset, id_literal.length, "Only numeric primitive types are supported as backend for the enum.");
        } else {
            p_parent_primitive = (ketl_type_primitive*)p_type;
        }
    }

    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_LEFT, defined_type ? "Expected '{' after enum parent type" : "Expected '{' after enum name.");

    ketl_type_enum_pair a_enum_constants[256] = {0};
    uint32_t enum_constants_count = 0;

    ketl_variable current_value = {
        .uint64 = 0,
    };
    ketl_variable_set_type(&current_value, (ketl_type*)p_parent_primitive);
    --current_value.uint64;

    do {
        if (token_check(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT)) {
            break;
        }

        ketl_token_t constant_name = CURRENT_TOKEN(0);
        a_enum_constants[enum_constants_count].s_name = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, TOKEN_STRING(constant_name), TOKEN_LENGTH(constant_name));
        token_advance(p_context); // id

        if (token_match(p_context, KETL_TOKEN_TYPE_ASSIGN)) {
            ketl_token_t constant = CURRENT_TOKEN(0);
            if (token_match(p_context, KETL_TOKEN_TYPE_LITERAL_INTEGER)) {
                char a_buffer[16] = {'\0'};
                ketl_memcpy(a_buffer, TOKEN_STRING(constant), TOKEN_LENGTH(constant));
                int64_t value = strtoll(a_buffer, NULL, 0);

                current_value.int64 = value;
            } else if (token_match(p_context, KETL_TOKEN_TYPE_ID)) {
                ketl_atomic_string s_constant_name = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, TOKEN_STRING(constant), TOKEN_LENGTH(constant));

                bool found = false;
                for (uint32_t i = 0u; i < enum_constants_count; ++i) {
                    if (a_enum_constants[i].s_name == s_constant_name) {
                        current_value = a_enum_constants[i].literal;

                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    ketl_hir_expr_info_t expr_info = token_extract_info(constant);
                    errorf(expr_info.source_offset, expr_info.length, "Unexpected '%.*s' for the enum constant value.", TOKEN_LENGTH(constant), TOKEN_STRING(constant));
                    ++current_value.uint64;
                }
            } else {
                ketl_hir_expr_info_t expr_info = token_extract_info(constant);
                errorf(expr_info.source_offset, expr_info.length, "Unexpected '%.*s' for the enum constant value.", TOKEN_LENGTH(constant), TOKEN_STRING(constant));
                ++current_value.uint64;
            }
        } else {
            ++current_value.uint64;
        }

        a_enum_constants[enum_constants_count].literal = current_value;

        ++enum_constants_count;
    } while (token_match(p_context, KETL_TOKEN_TYPE_COMMA));

    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT, "Expected '}' at the end of enum declaration.");
    
    ketl_state_define_enum(p_context->p_state, p_context->p_namespace, 
        TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal), p_parent_primitive, a_enum_constants, enum_constants_count, p_context->export);
    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_sharp_prefix(ketl_parser_context* p_context) {
    token_advance(p_context); // #
    token_consume(p_context, KETL_TOKEN_TYPE_ID, "Expected id after '#' prefix.");
    ketl_token_t id_literal = CURRENT_TOKEN(1);

    ketl_hir_expr_info_t expr_info = expr_info_merge(token_extract_info(CURRENT_TOKEN(2)), token_extract_info(id_literal));

    if (ketl_str_is_equal_n("entry", TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal))) {
        p_context->next_symbol_entry = true;
        return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
    }

    errorf(expr_info.source_offset, expr_info.length, "Unknown '#' command '%.*s'.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal));
    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_import(ketl_parser_context* p_context) {
    //ANN_ASSERT(p_context->is_global_scope);

    char a_path_buffer[256] = {'\0'};
    uint32_t path_length = 0;

    token_advance(p_context); // import
    ketl_token_t module_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // id
    while (token_match(p_context, KETL_TOKEN_TYPE_DOT)) {
        snprintf(a_path_buffer + path_length, ANN_ARRAY_SIZE(a_path_buffer) - path_length, "%.*s/", TOKEN_LENGTH(module_literal), TOKEN_STRING(module_literal));
        module_literal = CURRENT_TOKEN(0);
        token_advance(p_context); // module id
    }

    ketl_hir_expr_info_t expr_info = expr_info_merge(token_extract_info(CURRENT_TOKEN(2)), token_extract_info(CURRENT_TOKEN(1)));

    const char* p_module_name = TOKEN_STRING(module_literal);
    uint32_t module_name_length = TOKEN_LENGTH(module_literal);

    ketl_atomic_string s_module_name = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, p_module_name, module_name_length);

    if (!ketl_state_load_module_impl(p_context->p_state, s_module_name, a_path_buffer, p_context->p_namespace, p_context->export)) {
        errorf(expr_info.source_offset, expr_info.length, "Couldn't find module '%.*s'", TOKEN_LENGTH(module_literal), TOKEN_STRING(module_literal));
    }

    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_from_import(ketl_parser_context* p_context) {
    //ANN_ASSERT(p_context->is_global_scope);

    char a_path_buffer[256] = {'\0'};
    uint32_t path_length = 0;

    token_advance(p_context); // from
    ketl_token_t module_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // module id
    while (token_match(p_context, KETL_TOKEN_TYPE_DOT)) {
        snprintf(a_path_buffer + path_length, ANN_ARRAY_SIZE(a_path_buffer) - path_length, "%.*s/", TOKEN_LENGTH(module_literal), TOKEN_STRING(module_literal));
        module_literal = CURRENT_TOKEN(0);
        token_advance(p_context); // module id
    }
    token_consume(p_context, KETL_TOKEN_TYPE_IMPORT, "Expected 'import' keyword after module name.");
    ketl_token_t name_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // entity id

    ketl_hir_expr_info_t expr_info = expr_info_merge(token_extract_info(CURRENT_TOKEN(4)), token_extract_info(CURRENT_TOKEN(1)));

    const char* p_module_name = TOKEN_STRING(module_literal);
    uint32_t module_name_length = TOKEN_LENGTH(module_literal);

    ketl_atomic_string s_module_name = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, p_module_name, module_name_length);

    ketl_namespace* p_module_namespace = ketl_state_load_module_impl(p_context->p_state, s_module_name, a_path_buffer, NULL, p_context->export);
    if (p_module_namespace == NULL) {
        errorf(expr_info.source_offset, expr_info.length, "Couldn't find module '%.*s'.", TOKEN_LENGTH(module_literal), TOKEN_STRING(module_literal));
        return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
    }

    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, TOKEN_STRING(name_literal), TOKEN_LENGTH(name_literal));
    ketl_namespace_node* p_node = ketl_namespace_find(p_module_namespace, s_name);

    if (p_node == NULL) {
        errorf(expr_info.source_offset, expr_info.length, "Couldn't find '%.*s' in module '%.*s'.", 
            TOKEN_LENGTH(name_literal), TOKEN_STRING(name_literal),
            TOKEN_LENGTH(module_literal), TOKEN_STRING(module_literal)
        );
        return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
    }

    ketl_variable namespace_var = p_node->variable;

    ketl_namespace_node* p_local_node = ketl_namespace_put(p_context->p_namespace, s_name, KETL_ATOMIC_STRING_EMPTY, namespace_var, 
        (ketl_namespace_node_info){ .export = p_context->export, .imported = true }, &p_context->p_state->atomic_strings, false);
    p_local_node->s_name = p_node->s_name;

    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_var_declaration(ketl_parser_context* p_context) {
    ketl_hir_expr_info_t expr_info = token_extract_info(CURRENT_TOKEN(0));

    token_advance(p_context); // var
    ketl_token_t id_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // id
    ketl_hir_used_type_index_t type = KETL_HIR_USED_TYPE_UNKNOWN;
    ketl_hir_var_id_t initial_value = -1;
    if (token_match(p_context, KETL_TOKEN_TYPE_COLON)) {
        type = parse_type_as_index(p_context);
    }
    if (token_match(p_context, KETL_TOKEN_TYPE_ASSIGN)) {
        initial_value = parse_expression(p_context);
        expr_info = expr_info_merge(expr_info, GET_VAR(initial_value).expr_info);
        
        if (type == KETL_HIR_USED_TYPE_UNKNOWN) {
            type = GET_VAR(initial_value).type;
        } else {
            initial_value = trying_to_cast_rhs_to_lhs(p_context, type, expr_info_merge(expr_info, token_extract_info(id_literal)), initial_value, false);
        }
    } else {
        if (type == KETL_HIR_USED_TYPE_UNKNOWN) {
            expr_info = expr_info_merge(expr_info, token_extract_info(CURRENT_TOKEN(1)));
            errorf(expr_info.source_offset, expr_info.length, "Variable declaration without a type expects an initial expression.");

            initial_value = push_temp_var(p_context, expr_info);
        } else {
            ketl_type* p_type = GET_TYPE(type);
            initial_value = push_default_initial_value(p_context, p_type, token_extract_info(id_literal));
        }
    }
    if (type == KETL_HIR_USED_TYPE_UNKNOWN) {
        errorf(expr_info.source_offset, expr_info.length, "Can't deduce var type.");
        return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
    }
    if (type == KETL_HIR_USED_TYPE_LITERAL) {
        errorf(expr_info.source_offset, expr_info.length, "Using numeric literal as initial value not allowed without a declared type.");
        return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
    }
    push_hir_variable_declaration(p_context, id_literal, type, initial_value, expr_info);
    token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' after variable declaration.");
    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

// ffi interface
// TODO works only in asm printout, implement for the runtime
static ketl_statement_info parse_cimport_declaration(ketl_parser_context* p_context) {
    token_advance(p_context); // cimport
    ketl_token_t id_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // id

    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, "Expected '(' after function name.");

    ketl_named_variable_type_info_t a_function_parameters_named[256] = {0};
    ketl_variable_type_info_t a_function_parameters[256] = {0};
    ketl_function_parameters function_parameters = {
        .p_parameters = a_function_parameters,
        .parameters_count = 0,
    };

    if (!token_check(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT)) {
        do {
            ketl_token_t parameter_literal = CURRENT_TOKEN(0);
            a_function_parameters_named[function_parameters.parameters_count].p_name = TOKEN_STRING(parameter_literal);
            a_function_parameters_named[function_parameters.parameters_count].name_length = TOKEN_LENGTH(parameter_literal);
            token_advance(p_context); // id

            token_consume(p_context, KETL_TOKEN_TYPE_COLON, "Expected ':' after parameter name.");

            a_function_parameters_named[function_parameters.parameters_count].info.p_type = parse_type(p_context);
            a_function_parameters[function_parameters.parameters_count + 1] = a_function_parameters_named[function_parameters.parameters_count].info;

            ++function_parameters.parameters_count;
        } while (token_match(p_context, KETL_TOKEN_TYPE_COMMA));
    }
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after parameters.");

    ketl_type* return_type = NULL;
    if (token_match(p_context, KETL_TOKEN_TYPE_DOUBLE_ARROW_RIGHT)) {
        return_type = parse_type(p_context);
    }
    if (return_type == NULL) {
        return_type = ketl_state_get_none_type(p_context->p_state);
    }

    a_function_parameters[0].p_type = return_type;
    ++function_parameters.parameters_count;

    ketl_type* function_type = ketl_state_get_cfunction_type(p_context->p_state, &function_parameters);
    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal));
    ketl_state_define_cfunction(p_context->p_state, p_context->p_namespace, 
        s_name, s_name, function_type, NULL, p_context->export);

    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_function_declaration(ketl_parser_context* p_context) {
    token_advance(p_context); // fn
    ketl_token_t id_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // id

    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, "Expected '(' after function name.");

    ketl_named_variable_type_info_t a_function_parameters_named[256] = {0};
    ketl_variable_type_info_t a_function_parameters[256] = {0};
    ketl_function_parameters function_parameters = {
        .p_parameters = a_function_parameters,
        .parameters_count = 0,
    };

    if (!token_check(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT)) {
        do {
            ketl_token_t parameter_literal = CURRENT_TOKEN(0);
            a_function_parameters_named[function_parameters.parameters_count].p_name = TOKEN_STRING(parameter_literal);
            a_function_parameters_named[function_parameters.parameters_count].name_length = TOKEN_LENGTH(parameter_literal);
            token_advance(p_context); // id

            token_consume(p_context, KETL_TOKEN_TYPE_COLON, "Expected ':' after parameter name.");

            a_function_parameters_named[function_parameters.parameters_count].info.p_type = parse_type(p_context);
            a_function_parameters[function_parameters.parameters_count + 1] = a_function_parameters_named[function_parameters.parameters_count].info;

            ++function_parameters.parameters_count;
        } while (token_match(p_context, KETL_TOKEN_TYPE_COMMA));
    }
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after parameters.");

    ketl_type* return_type = NULL;
    if (token_match(p_context, KETL_TOKEN_TYPE_DOUBLE_ARROW_RIGHT)) {
        return_type = parse_type(p_context);
    }
    if (return_type == NULL) {
        return_type = ketl_state_get_none_type(p_context->p_state);
    }

    uint32_t parameters_count = function_parameters.parameters_count;
    a_function_parameters[0].p_type = return_type;
    ++function_parameters.parameters_count;
    
    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_LEFT, "Expected '{' after function declaration.");

    // TODO use function instead cfunction
    ketl_type* function_type = ketl_state_get_cfunction_type(p_context->p_state, &function_parameters);
    ketl_atomic_string s_name = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal));
    ketl_namespace_node* p_func_node = ketl_state_define_cfunction(p_context->p_state, p_context->p_namespace, 
        s_name, p_context->cexport ? s_name : KETL_ATOMIC_STRING_EMPTY, function_type, NULL, p_context->export);
    ketl_namespace* p_direct_namespace = ketl_namespace_find_direct_parent(p_context->p_namespace, p_func_node);
    ANN_ASSERT(p_direct_namespace == p_context->p_namespace);
    uint32_t namespace_node_index = ketl_namespace_get_index(p_direct_namespace, p_func_node);
    
    // looking for the end of the function definition
    ketl_token_iterator_t start_pos = p_context->p_lexer->token_iterator;
    ketl_parser_bracket_t a_brackets_stack[32];
    uint32_t bracket_stack_count = 0;

    dummy_parse_bracket(KETL_TOKEN_TYPE_CURLY_LEFT, a_brackets_stack, &bracket_stack_count);
    while (bracket_stack_count > 0) {
        ketl_token_t token = CURRENT_TOKEN(0);
        ketl_token_type current_token_type = token.type;
        if (current_token_type == KETL_TOKEN_TYPE_EOF) {
            errorf(token.offset, token.length, "Expected operator.");
            return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
        }

        dummy_parse_bracket(current_token_type, a_brackets_stack, &bracket_stack_count);
        token_advance(p_context);
    }

    // TODO do proper error evaluation during this pass
    //token_consume(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT, "Expected '}' after function body.");

    // saving end of the function definition for later
    ketl_token_iterator_t end_pos = p_context->p_lexer->token_iterator;

    compile_function_declaration_t function_decl = {
        .p_namespace = p_direct_namespace,
        .namespace_node_index = namespace_node_index,
        .p_opcodes = NULL,
        .opcodes_size = 0,
        .start_pos = start_pos,
        .end_pos = end_pos,
    };
    ketl_parameters_t_init(&function_decl.v_parameters, 4, p_context->p_state->p_allocator);
    for (uint32_t i = 0; i < parameters_count; ++i) {
        ketl_parameters_t_push_back_ref(&function_decl.v_parameters, &a_function_parameters_named[i]);
    }
    compile_function_declarations_t_push_back_ref(&p_context->p_state->compile_function_declarations, &function_decl);

    if (p_context->next_symbol_entry) {
        p_context->hir_builder.s_entry = p_func_node->s_name;
        p_context->next_symbol_entry = false;
    }

    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static void parse_class_declaration_inner(ketl_parser_context* p_context, ketl_named_variable_type_info_t* p_class_fields, uint32_t* p_class_field_count);

static ketl_statement_info parse_class_declaration(ketl_parser_context* p_context) {
    token_advance(p_context); // class
    ketl_token_t id_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // id

    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_LEFT, "Expected '{' after class name.");

    ketl_namespace* p_class_namespace;
    ketl_namespace_node* p_class_node = ketl_state_forward_define_class(p_context->p_state, p_context->p_namespace, 
        TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal), &p_class_namespace, p_context->export);

    ketl_namespace* p_old_namespace = p_context->p_namespace;
    p_context->p_namespace = p_class_namespace;

    bool old_export = p_context->export;
    p_context->export = false;

    push_symbol_map(p_context);

    ketl_named_variable_type_info_t a_class_fields[256] = {0};
    uint32_t class_field_count = 0;

    while (!token_check(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT) && !token_check(p_context, KETL_TOKEN_TYPE_EOF)) {
        parse_class_declaration_inner(p_context, a_class_fields, &class_field_count);
    }

    pop_symbol_map(p_context);
    
    p_context->export = old_export;
    p_context->p_namespace = p_old_namespace;

    ketl_state_post_define_class(p_context->p_state, p_class_node, a_class_fields, class_field_count);
    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT, "Expected '}' in the end of a class declaration.");
    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_mimic_declaration(ketl_parser_context* p_context) {
    token_advance(p_context); // mimic
    ketl_token_t id_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // id

    token_consume(p_context, KETL_TOKEN_TYPE_ASSIGN, "Expected '=' after mimic name.");

    ketl_type* p_type = parse_type(p_context);
    ketl_state_define_mimic(p_context->p_state, p_context->p_namespace, 
        TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal), p_type, p_context->export);
    token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' in the end of a mimin declaration.");
    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static void parse_class_declaration_inner(ketl_parser_context* p_context, ketl_named_variable_type_info_t* p_class_fields, uint32_t* p_class_field_count) {
    ANN_FOREVER switch (CURRENT_TOKEN(0).type) {
        case KETL_TOKEN_TYPE_TERMINATION_CHARACTER: 
            token_advance(p_context); // ;
            return;
        case KETL_TOKEN_TYPE_VAR    : {
            token_advance(p_context); // var
            ketl_token_t field_literal = CURRENT_TOKEN(0);
            p_class_fields[*p_class_field_count].p_name = TOKEN_STRING(field_literal);
            p_class_fields[*p_class_field_count].name_length = TOKEN_LENGTH(field_literal);
            token_advance(p_context); // id

            token_consume(p_context, KETL_TOKEN_TYPE_COLON, "Expected ':' after field name.");

            p_class_fields[*p_class_field_count].info.p_type = parse_type(p_context);

            token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' after field declaration.");

            ++(*p_class_field_count);
            return;
        }
        case KETL_TOKEN_TYPE_SHARED: {
            parse_var_declaration(p_context);
            return;
        }
        case KETL_TOKEN_TYPE_EXTEND: {
            token_advance(p_context); // extend
            p_class_fields[*p_class_field_count].info.p_type = parse_type(p_context);

            token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' after field declaration.");

            ++(*p_class_field_count);
            return;
        }
        case KETL_TOKEN_TYPE_CIMPORT: parse_cimport_declaration (p_context); return;
        case KETL_TOKEN_TYPE_FN     : parse_function_declaration(p_context); return;
        case KETL_TOKEN_TYPE_CLASS  : parse_class_declaration   (p_context); return;
        case KETL_TOKEN_TYPE_ENUM   : parse_enum_declaration    (p_context); return;
        case KETL_TOKEN_TYPE_MIMIC  : parse_mimic_declaration   (p_context); return;
        case KETL_TOKEN_TYPE_EXPORT : {
            token_advance(p_context);
            if (p_context->export) {
                ketl_hir_expr_info_t expr_info = token_extract_info(CURRENT_TOKEN(1));
                errorf(expr_info.source_offset, expr_info.length, "Excessive 'export' operator.");
                continue;
            }
            p_context->export = true;
            parse_class_declaration_inner(p_context, p_class_fields, p_class_field_count);
            p_context->export = false;
            return;
        }
        case KETL_TOKEN_TYPE_CEXPORT: {
            token_advance(p_context);
            if (p_context->cexport) {
                ketl_hir_expr_info_t expr_info = token_extract_info(CURRENT_TOKEN(1));
                errorf(expr_info.source_offset, expr_info.length, "Excessive 'cexport' operator.");
                continue;
            }
            p_context->cexport = true;
            parse_class_declaration_inner(p_context, p_class_fields, p_class_field_count);
            p_context->cexport = false;
            return;
        }
        case KETL_TOKEN_TYPE_IMPORT : 
            errorf(CURRENT_TOKEN(0).offset, CURRENT_TOKEN(0).length, "'import' is not supported inside class declaration.");
            parse_import(p_context);
            return;
        case KETL_TOKEN_TYPE_FROM   : 
            errorf(CURRENT_TOKEN(0).offset, CURRENT_TOKEN(0).length, "'import' is not supported inside class declaration.");
            parse_from_import(p_context);
            return;
        default: 
            errorf(CURRENT_TOKEN(0).offset, CURRENT_TOKEN(0).length, "Statements are not supported inside class declaration.");
            parse_statement(p_context);
            return;
    }
}

static ketl_statement_info parse_declaration(ketl_parser_context* p_context) {
    ANN_FOREVER switch (CURRENT_TOKEN(0).type) {
        case KETL_TOKEN_TYPE_SHARP  : return parse_sharp_prefix        (p_context);
        case KETL_TOKEN_TYPE_IMPORT : return parse_import              (p_context);
        case KETL_TOKEN_TYPE_FROM   : return parse_from_import         (p_context);
        case KETL_TOKEN_TYPE_CIMPORT: return parse_cimport_declaration (p_context);
        case KETL_TOKEN_TYPE_VAR    : return parse_var_declaration     (p_context);
        case KETL_TOKEN_TYPE_SHARED : errorf(token_extract_info(CURRENT_TOKEN(0)).source_offset, token_extract_info(CURRENT_TOKEN(0)).length, "Can't use 'shared' outside class declaration.");
                                      return parse_var_declaration     (p_context);
        case KETL_TOKEN_TYPE_FN     : return parse_function_declaration(p_context);
        case KETL_TOKEN_TYPE_CLASS  : return parse_class_declaration   (p_context);
        case KETL_TOKEN_TYPE_ENUM   : return parse_enum_declaration    (p_context);
        case KETL_TOKEN_TYPE_MIMIC  : return parse_mimic_declaration   (p_context);
        case KETL_TOKEN_TYPE_EXPORT : {
            token_advance(p_context);
            if (p_context->export) {
                ketl_hir_expr_info_t expr_info = token_extract_info(CURRENT_TOKEN(1));
                errorf(expr_info.source_offset, expr_info.length, "Excessive 'export' operator.");
                continue;
            }
            p_context->export = true;
            ketl_statement_info result = parse_declaration(p_context);
            p_context->export = false;
            return result;
        }
        case KETL_TOKEN_TYPE_CEXPORT: {
            token_advance(p_context);
            if (p_context->cexport) {
                ketl_hir_expr_info_t expr_info = token_extract_info(CURRENT_TOKEN(1));
                errorf(expr_info.source_offset, expr_info.length, "Excessive 'cexport' operator.");
                continue;
            }
            p_context->cexport = true;
            ketl_statement_info result = parse_declaration(p_context);
            p_context->cexport = false;
            return result;
        }
        default                     : return parse_statement           (p_context);
    }
}

void ketl_parser_build_hir(ketl_state* p_state, ketl_hir_t* p_hir, ketl_lexer_t* p_lexer, ketl_token_iterator_t end_pos, ketl_namespace* p_namespace, ketl_named_variable_type_info_t* p_parameters, uint32_t parameter_count, ketl_type* p_return_type, uint16_t func_index, bool is_global_scope, const ketl_allocator* p_allocator) {
    *p_hir = (ketl_hir_t){0};

    ketl_parser_context context = {0};
    context = (ketl_parser_context){
        .p_state = p_state, 
        .p_lexer = p_lexer,
        .p_namespace = p_namespace,
        .end_pos = end_pos,
        .reserved_break = -1,
        .reserved_continue = -1,
        .is_global_scope = is_global_scope,
    };
    ketl_parser_context* p_context = &context;

    ketl_hir_builder_init(&context.hir_builder, p_state, p_lexer, p_return_type, func_index, p_allocator);
    context.s_filename = push_symbol_string(&context, 
        ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_lexer->s_filename), KETL_NULL_TERMINATED_LENGTH_32);

    for (uint32_t i = 0u; i < parameter_count; ++i) {
        ketl_hir_builder_add_parameter(&context.hir_builder, p_namespace, &p_parameters[i]);
    }
    
    if (p_lexer->tokens.size > 0) {
        _ketl_parse_argument_stack_t_init(&context.v_argument_stack, 4, p_allocator);
        _ketl_parse_undefined_vars_infos_t_init(&context.m_undef_vars_infos, p_allocator);

        ketl_statement_info statement_info = parse_block_statement_inner(&context);
        if ((statement_info.return_info & KETL_RETURN_UNDEF) == KETL_RETURN_UNDEF ||
            (!(statement_info.return_info & KETL_RETURN_ALWAYS) && (
                (statement_info.return_info & KETL_RETURN_VALUE) || (p_return_type != NULL && !ketl_type_is_none_type(p_return_type))
            ))) {
            // TODO POS?
            errorf(p_lexer->tokens.p_data[end_pos - 1].offset, 1, "Not all control returns value.");
        }

        if (p_context->reserved_break != (ketl_hir_block_index_t)-1 ||
            p_context->reserved_continue != (ketl_hir_block_index_t)-1) {
            errorf(p_lexer->tokens.p_data[end_pos - 1].offset, 1, "Expected statement.");
        }

        if (!(statement_info.return_info & KETL_RETURN_ALWAYS)) {
            // we need to return eventually
            push_hir_instr(p_context, KETL_HIR_RETURN);
        }

        _ketl_parse_undefined_vars_infos_t_deinit(&context.m_undef_vars_infos);
        _ketl_parse_argument_stack_t_deinit(&context.v_argument_stack);
    }

    ketl_hir_builder_flush(&context.hir_builder, p_hir);

    return;
}
