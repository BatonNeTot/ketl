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

ANN_DEFINE(ketl_parse_pos_info) {
    uint32_t start_pos_line;
    uint32_t end_pos_line;
    uint16_t start_pos_col;
    uint16_t end_pos_col;
};

typedef uint16_t lalr_state;

KETL_VECTOR_DECLARATION(_ketl_parse_argument_stack_t, ketl_hir_var_id_t)
KETL_VECTOR_DEFINITION(_ketl_parse_argument_stack_t, ketl_hir_var_id_t)

ANN_DEFINE(ketl_parser_context) {
    ketl_state* p_state;
    ketl_lexer_t* p_lexer;
    ketl_namespace* p_namespace;
    ketl_token_iterator_t end_pos;
    ketl_hir_symbol_offset_t s_filename;
    ketl_hir_builder_t hir_builder;

    _ketl_parse_argument_stack_t argument_stack;

    bool vars_in_namespace;
    bool export;
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

static ketl_token_t get_current_token(ketl_parser_context* p_context, ketl_token_iterator_t offset) {
    ketl_token_iterator_t index = (p_context)->p_lexer->token_iterator - (offset);
    if (index >= p_context->end_pos) {
        return (ketl_token_t){
            .type = KETL_TOKEN_TYPE_EOF, 
            .length = (ketl_token_length_t)0, 
            .offset = TOKEN(index).offset,
        };
    }
    return TOKEN(index);
}

static ketl_hir_symbol_offset_t push_symbol_string(ketl_parser_context* p_context, const char* p_str, uint32_t length) {
    return (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_context->hir_builder.symbols, p_str, length);
}

static ketl_hir_symbol_offset_t push_symbol(ketl_parser_context* p_context, ketl_token_t literal) {
    return push_symbol_string(p_context, TOKEN_STRING(literal), TOKEN_LENGTH(literal));
}

static ketl_hir_var_id_t push_literal_number_symbol_of_type(ketl_parser_context* p_context, ketl_hir_symbol_offset_t literal, ketl_hir_used_type_index_t type) {
    ketl_hir_var_id_t literal_var = ketl_hir_builder_get_literal(&p_context->hir_builder, literal, type);
    return literal_var;
}

static ketl_hir_var_id_t push_literal_number_symbol(ketl_parser_context* p_context, ketl_hir_symbol_offset_t literal) {
    // TODO determine correct type
    ketl_type* p_type = ketl_state_get_i64(p_context->p_state);
    ketl_hir_used_type_index_t type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_type);
    return push_literal_number_symbol_of_type(p_context, literal, type);
}

static ketl_hir_var_id_t push_literal_number(ketl_parser_context* p_context, ketl_token_t literal) {
    return push_literal_number_symbol(p_context, push_symbol(p_context, literal));
}


static ketl_hir_var_id_t push_temp_var_type(ketl_parser_context* p_context, ketl_hir_used_type_index_t type) {
    return ketl_hir_builder_create_temp_var(&p_context->hir_builder, type);
}
static ketl_hir_var_id_t push_temp_var(ketl_parser_context* p_context) {
    return push_temp_var_type(p_context, KETL_HIR_USED_TYPE_UNKNOWN);
}

static ketl_hir_var_id_t push_hir_binary_op(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_tag_t hir_tag, ketl_hir_var_id_t lhs, ketl_hir_var_id_t rhs) {
    (void)p_pos_info;
    ketl_hir_header_t header = {
        .tag = hir_tag,
        .file_symbol = p_context->s_filename,
        /*
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
        */
    };
    ketl_hir_var_id_t output_var = push_temp_var(p_context); 
    ketl_hir_binary_op_t instr = {
        .output_var = output_var,
        .lhs_var = lhs,
        .rhs_var = rhs,
    };
    ketl_hir_builder_insert_binary_op(&p_context->hir_builder, header, &instr);
    return output_var;
}

static void push_hir_assign_impl(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t lhs_var, ketl_hir_var_id_t rhs_var) {

    ketl_hir_var_t* p_lhs_var = p_context->hir_builder.vars.p_data + lhs_var;
    ketl_hir_var_t* p_rhs_var = p_context->hir_builder.vars.p_data + rhs_var;

    if (p_lhs_var->uid == KETL_HIR_VAR_UID_LITERAL) {
        ANN_ASSERT(false && "Can't assign to an r-value.");
    }

    if (p_rhs_var->type == KETL_HIR_USED_TYPE_UNKNOWN) {
        ANN_ASSERT(false && "Can't assign undefined value.");
    }

    // TODO casting if needed
    if (p_lhs_var->type != p_rhs_var->type) {
        ketl_type* p_lhs_type = p_context->hir_builder.used_types.p_data[p_lhs_var->type];
        ketl_type* p_rhs_type = p_context->hir_builder.used_types.p_data[p_rhs_var->type];

        if (!((p_lhs_type->kind == KETL_TYPE_ARRAY || p_lhs_type->kind == KETL_TYPE_FUNCTION || p_lhs_type->kind == KETL_TYPE_CFUNCTION || p_lhs_type->kind == KETL_TYPE_CLASS) &&
            // hack to check for raw type
            p_rhs_type->kind == KETL_TYPE_PRIMITIVE && p_rhs_type->size == sizeof(void*)) &&
            !((p_rhs_type->kind == KETL_TYPE_ARRAY || p_rhs_type->kind == KETL_TYPE_FUNCTION || p_rhs_type->kind == KETL_TYPE_CFUNCTION || p_rhs_type->kind == KETL_TYPE_CLASS) &&
            // hack to check for raw type
            p_lhs_type->kind == KETL_TYPE_PRIMITIVE && p_lhs_type->size == sizeof(void*))) {
            ANN_ASSERT(false && "Incompatible assignment types.");
        }
    }

    if (p_rhs_var->uid != KETL_HIR_VAR_UID_LITERAL && p_rhs_var->info == KETL_HIR_VAR_INFO_TEMP) {
        ketl_hir_builder_replace_temp_var(&p_context->hir_builder, lhs_var, rhs_var);
        return;
    }

    (void)p_pos_info;
    ketl_hir_header_t assign_header = {
        // TODO fix size
        .tag = KETL_HIR_ASSIGN | KETL_HIR_I64,
        .file_symbol = p_context->s_filename,
        /*
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
        */
    };
    ketl_hir_assign_t instr = {
        .dest_var = lhs_var,
        .source_var = rhs_var,
    };
    ketl_hir_builder_insert_instr(&p_context->hir_builder, assign_header, (uint8_t*)&instr);
}

static ketl_hir_var_id_t push_hir_assign(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t lhs_var, ketl_hir_var_id_t rhs_var) {
    ketl_hir_var_t* p_lhs_var = p_context->hir_builder.vars.p_data + lhs_var;
    if (p_lhs_var->uid == KETL_HIR_VAR_UID_LITERAL) {
        // TODO POS
        errorf(0, 1, "Can't assign to an l-value.");
        return push_temp_var(p_context);
    }

    if (p_lhs_var->info != KETL_HIR_VAR_INFO_TEMP 
        && p_lhs_var->uid != KETL_HIR_VAR_UID_GLOBAL
        && p_lhs_var->uid != KETL_HIR_VAR_UID_FIELD) {
        // TODO will not work in a looping scenario without phi instruction at the begining of the block
        //lhs_var = ketl_hir_builder_increment_var_uid(&p_context->hir_builder, lhs_var);
        // TODO should not run during debug compilation
    }
    push_hir_assign_impl(p_context, p_pos_info, lhs_var, rhs_var);

    return lhs_var;
}

static void push_hir_return_value(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t var_id) {
    (void)p_pos_info;
    ketl_hir_header_t header = {
        // TODO fix size
        .tag = KETL_HIR_RETURN_VALUE | KETL_HIR_I64,
        .file_symbol = p_context->s_filename,
        /*
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
        */
    };
    ketl_hir_return_value_t instr = {
        .value_var = var_id,
    };
    ketl_hir_builder_insert_instr(&p_context->hir_builder, header, (uint8_t*)&instr);
}

static ketl_hir_var_id_t push_literal_id(ketl_parser_context* p_context, ketl_token_t literal) {
    // TODO decide how to update uids
    ketl_hir_var_id_t id_var = ketl_hir_builder_get_var(&p_context->hir_builder, p_context->p_namespace, 
        push_symbol(p_context, literal), KETL_HIR_USED_TYPE_UNKNOWN);
    if (id_var == (ketl_hir_var_id_t)-1) {
        errorf(literal.offset, literal.length, "Use of undeclared variable '%.*s'.", TOKEN_LENGTH(literal), TOKEN_STRING(literal));
        id_var = push_temp_var(p_context);
    }
    return id_var;
}

static ketl_hir_var_id_t push_array_index(ketl_parser_context* p_context, ketl_hir_var_id_t array_id, ketl_hir_var_id_t arg_id) {
    // TODO decide how to update uids
    ketl_hir_var_id_t id_var = ketl_hir_builder_create_index_var(&p_context->hir_builder, array_id, arg_id);
    return id_var;
}

static ketl_hir_var_id_t push_object_field(ketl_parser_context* p_context, ketl_hir_var_id_t object_id, ketl_token_t literal) {
    // TODO decide how to update uids
    ketl_hir_var_id_t id_var = ketl_hir_builder_create_field_var(&p_context->hir_builder, object_id,
        push_symbol(p_context, literal), KETL_HIR_USED_TYPE_UNKNOWN);
    if (id_var == (ketl_hir_var_id_t)-1) {
        errorf(literal.offset, literal.length, "Use of undeclared field '%.*s'.", TOKEN_LENGTH(literal), TOKEN_STRING(literal));
        id_var = push_temp_var(p_context);
    }
    return id_var;
}

static ketl_hir_used_type_index_t find_type(ketl_parser_context* p_context, ketl_token_t type_literal) {
    ketl_type* p_type = ketl_state_get_type_impl(p_context->p_state, p_context->p_namespace, TOKEN_STRING(type_literal), TOKEN_LENGTH(type_literal));
    return ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_type);
}

static void push_hir_variable_declaration(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_token_t id_literal, ketl_hir_used_type_index_t type_index, ketl_hir_var_id_t init_var) {
    ketl_hir_var_id_t id_var;
    if (p_context->vars_in_namespace) {
        ketl_namespace_node* p_namespace_node = ketl_state_define_var(p_context->p_state, p_context->p_namespace, 
            TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal), p_context->hir_builder.used_types.p_data[type_index]);
        p_namespace_node->export = p_context->export;
        id_var = ketl_hir_builder_get_global_var(&p_context->hir_builder, p_namespace_node, push_symbol(p_context, id_literal), type_index);
    } else {
        id_var = ketl_hir_builder_register_var(&p_context->hir_builder, p_context->p_namespace, 
            push_symbol(p_context, id_literal), type_index);
    }

    push_hir_assign_impl(p_context, p_pos_info, id_var, init_var);
}

static void push_hir_argument(ketl_parser_context* p_context, ketl_hir_var_id_t var_id) {
    _ketl_parse_argument_stack_t_push_back_copy(&p_context->argument_stack, var_id);
}

static ketl_hir_var_id_t push_hir_call(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t callee_id, uint16_t arguments_count) {
    (void)p_pos_info;
    ketl_hir_header_t header = {
        .tag = KETL_HIR_CALL,
        .file_symbol = p_context->s_filename,
        /*
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
        */
    };
    ketl_hir_var_id_t output_var = push_temp_var(p_context); 
    ketl_hir_call_t instr = {
        .output_var = output_var,
        .callee = callee_id,
        .arguments_count = arguments_count,
    };

    ketl_hir_builder_insert_call(&p_context->hir_builder, header, &instr,
        // pass pointer to last 'arguments_count' elements and immidiatly cut 'arguments_count' tail
        p_context->argument_stack.p_data + (p_context->argument_stack.size -= arguments_count));
    return output_var;
}

static ketl_hir_var_id_t push_hir_new(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_used_type_index_t type, uint16_t arguments_count) {
    (void)p_pos_info;
    ketl_hir_header_t header = {
        .tag = KETL_HIR_NEW,
        .file_symbol = p_context->s_filename,
        /*
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
        */
    };
    ketl_hir_var_id_t output_var = push_temp_var_type(p_context, type); 
    ketl_hir_new_t instr = {
        .output_var = output_var,
        .type = type,
        .arguments_count = arguments_count,
    };

    ketl_hir_builder_insert_new(&p_context->hir_builder, header, &instr,
        // pass pointer to last 'arguments_count' elements and immidiatly cut 'arguments_count' tail
        p_context->argument_stack.p_data + (p_context->argument_stack.size -= arguments_count));
    return output_var;
}

static ketl_hir_var_id_t push_hir_create_array(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_used_type_index_t array_type, ketl_hir_var_id_t count_var_id) {
    (void)p_pos_info;
    ketl_hir_header_t header = {
        .tag = KETL_HIR_CREATE_ARRAY,
        .file_symbol = p_context->s_filename,
        /*
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
        */
    };
    ketl_hir_var_id_t output_var = push_temp_var_type(p_context, array_type); 
    ketl_hir_create_array_t instr = {
        .output_var = output_var,
        .type = array_type,
        .count_var_id = count_var_id,
    };

    ketl_hir_builder_insert_instr(&p_context->hir_builder, header, (uint8_t*)&instr);
    return output_var;
}

static ketl_hir_block_index_t reserve_hir_blocks(ketl_parser_context* p_context, uint8_t count) {
    ketl_hir_block_index_t first_block = (ketl_hir_block_index_t)p_context->hir_builder.blocks.size;
    hir_builder_blocks_t_reserve(&p_context->hir_builder.blocks, p_context->hir_builder.blocks.size + count);
    for (uint8_t i = count; i > 0; --i) {
        hir_builder_blocks_t_push_back_copy(&p_context->hir_builder.blocks, 0u);
    }
    return first_block;
}
static void pull_hir_set_block(ketl_parser_context* p_context, ketl_hir_block_index_t block) {
    p_context->hir_builder.blocks.p_data[block] = p_context->hir_builder.instrs.size;
    hir_builder_offset_to_block_t_get_or_insert_copy(&p_context->hir_builder.offset_to_block, p_context->hir_builder.instrs.size, block);
}

static void push_hir_if(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t bool_expr_var, ketl_hir_block_index_t true_statement, ketl_hir_block_index_t false_statement) {
    // TODO token_check casting
    
    (void)p_pos_info;
    ketl_hir_header_t if_header = {
        .tag = KETL_HIR_JUMP_IF_TRUE,
        .file_symbol = p_context->s_filename,
        /*
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
        */
    };
    ketl_hir_jump_if_t instr = {
        .true_block = true_statement,
        .false_block = false_statement,
        .expr_var = bool_expr_var,
    };
    
    // TODO provide true and false branches
    ketl_hir_builder_insert_instr(&p_context->hir_builder, if_header, (uint8_t*)&instr);
}

static void push_hir_jump(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_block_index_t target) {
    (void)p_pos_info;
    ketl_hir_header_t jump_header = {
        .tag = KETL_HIR_JUMP,
        .file_symbol = p_context->s_filename,
        /*
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
        */
    };
    ketl_hir_jump_t instr = {
        .block_index = target,
    };
    
    ketl_hir_builder_insert_instr(&p_context->hir_builder, jump_header, (uint8_t*)&instr);
}

static void push_hir_instr(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_tag_t tag) {
    (void)p_pos_info;
    ketl_hir_header_t header = {
        .tag = tag,
        .file_symbol = p_context->s_filename,
        /*
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
        */
    };
    ketl_hir_builder_insert_instr(&p_context->hir_builder, header,  NULL);
}

typedef ketl_hir_var_id_t(*ketl_parse_prefix)(ketl_parser_context*);
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
    KETL_PREC_TERM,
    KETL_PREC_FACTOR,
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
    [KETL_PREC_EQUALITY] = KETL_LTR,
    [KETL_PREC_COMPARISON] = KETL_LTR,
    [KETL_PREC_TERM] = KETL_LTR,
    [KETL_PREC_FACTOR] = KETL_LTR,
    [KETL_PREC_PRIMARY] = KETL_LTR,
};

ANN_DEFINE(ketl_parse_rule) {
    ketl_parse_prefix f_prefix;
    ketl_parse_ltr_infix f_ltr_infix;
    ketl_parse_rtl_infix f_rtl_infix;
    ketl_precedence precedence;
};

static ketl_parse_rule* get_parse_rule(ketl_token_type token_type);
static ketl_hir_var_id_t parse_precedence(ketl_parser_context* p_context, ketl_precedence precedence);

static void token_advance(ketl_parser_context* p_context) {
    ANN_FOREVER {
        uint32_t current = ++p_context->p_lexer->token_iterator;

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

static ketl_hir_var_id_t parse_null(ketl_parser_context* p_context) {
    ketl_hir_symbol_offset_t literal = push_symbol(p_context, CURRENT_TOKEN(1));
    ketl_type* p_type = ketl_state_get_raw_type(p_context->p_state);
    ketl_hir_used_type_index_t type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_type);
    return push_literal_number_symbol_of_type(p_context, literal, type);
}

static ketl_hir_var_id_t parse_number(ketl_parser_context* p_context) {
    return push_literal_number(p_context, CURRENT_TOKEN(1));
}

static ketl_hir_var_id_t parse_identificator(ketl_parser_context* p_context) {
    return push_literal_id(p_context, CURRENT_TOKEN(1));
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
    ketl_hir_var_t* p_callee = &p_context->hir_builder.vars.p_data[callee];
    if (p_callee->type == KETL_HIR_USED_TYPE_META) {
        ketl_hir_var_info_t* p_callee_info = &p_context->hir_builder.vars_infos.p_data[p_callee->info];
        ketl_namespace_node* p_callee_node = p_callee_info->p_global;
        if (p_callee_node->variable.kind != KETL_VARIABLE_TYPE) {
            errorf(0, 1, "Trying to call an improper object.");
            return push_temp_var(p_context);
        }
        ketl_type* p_callee_type = p_callee_node->variable.p_pointer;
        if (p_callee_type->kind != KETL_TYPE_CLASS) {
            errorf(0, 1, "Only class objects can be constructed through calling a type.");
            return push_temp_var(p_context);
        }
        
        return push_hir_new(p_context, NULL, 
            ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_callee_type), argument_count);
    } else {
        return push_hir_call(p_context, NULL, callee, argument_count);
    }
}

static ketl_hir_var_id_t parse_dot_operator(ketl_parser_context* p_context, ketl_hir_var_id_t lhs) {
    token_consume(p_context, KETL_TOKEN_TYPE_ID, "Expected id after access operator.");
    ketl_token_t id_literal = CURRENT_TOKEN(1);

    ketl_hir_var_t* p_object = &p_context->hir_builder.vars.p_data[lhs];
    if (p_object->type == KETL_HIR_USED_TYPE_META) {
        ketl_namespace_node* p_namespace_node = p_context->hir_builder.vars_infos.p_data[p_object->info].p_global;
        
        if (p_namespace_node->variable.kind == KETL_VARIABLE_NAMESPACE) { 
            ketl_namespace* p_namespace = p_namespace_node->variable.p_pointer;
            
            ketl_hir_var_id_t id_var = ketl_hir_builder_get_var(&p_context->hir_builder, p_namespace, 
                push_symbol(p_context, id_literal), KETL_HIR_USED_TYPE_UNKNOWN);
            if (id_var == (ketl_hir_var_id_t)-1) {
                errorf(id_literal.offset, id_literal.length, "Use of undeclared variable '%.*s' from module '%s'.", TOKEN_LENGTH(id_literal), TOKEN_STRING(id_literal),
                    ketl_atomic_strings_get_pointer(&p_context->p_state->atomic_strings, p_namespace->s_name));
                id_var = push_temp_var(p_context);
            }

            return id_var;
        }

        if (p_namespace_node->variable.kind == KETL_VARIABLE_TYPE) {
            ketl_type* p_type = p_namespace_node->variable.p_pointer;
            
            if (ketl_str_is_equal_n("size", TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal))) {
                uint64_t type_size = ketl_type_get_size(p_type);

                char a_buffer[256];
                uint32_t length = (uint32_t)snprintf(a_buffer, ANN_ARRAY_SIZE(a_buffer), "%"PRIu64, type_size);

                return push_literal_number_symbol(p_context, push_symbol_string(p_context, a_buffer, length));
            }

            ANN_ASSERT(false && "This type does not have this field");
        }

        ANN_ASSERT(false && "Trying to field access no-type and no-namespace");
        return -1;
    } else {
        return push_object_field(p_context, lhs, id_literal);
    }
}

static ketl_hir_var_id_t parse_indexing(ketl_parser_context* p_context, ketl_hir_var_id_t var_id) {
    ketl_hir_var_id_t expr_id = parse_expression(p_context);
    token_consume(p_context, KETL_TOKEN_TYPE_SQUARE_RIGHT, "Expected ']' after expression.");

    ketl_hir_var_t* p_var = &p_context->hir_builder.vars.p_data[var_id];
    if (p_var->type == KETL_HIR_USED_TYPE_META) {
        ketl_type* p_value_type = p_context->hir_builder.vars_infos.p_data[p_var->info].p_global->variable.p_pointer;
        ketl_hir_var_id_t id_var = push_hir_create_array(p_context, NULL, 
            ketl_hir_builder_get_used_type_index(&p_context->hir_builder, ketl_state_get_array_type(p_context->p_state, p_value_type)), expr_id);

        if (!token_match(p_context, KETL_TOKEN_TYPE_CURLY_LEFT)) {
            return id_var;
        }

        uint64_t index = 0;
        if (!token_check(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT)) {
            do {
                ketl_hir_var_id_t value_var = parse_expression(p_context);
                char a_literal_buffer[256] = {0};
                uint32_t literal_length = snprintf(a_literal_buffer, ANN_ARRAY_SIZE(a_literal_buffer), "%"PRIu64, index);
                ketl_hir_var_id_t index_literal = push_literal_number_symbol(p_context, push_symbol_string(p_context, a_literal_buffer, literal_length));
                ketl_hir_var_id_t indexed_var = push_array_index(p_context, id_var, index_literal);

                push_hir_assign(p_context, NULL, indexed_var, value_var);
                ++index;
            } while (token_match(p_context, KETL_TOKEN_TYPE_COMMA));
        }
        
        token_consume(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT, "Expected '}' after array initial values.");
        return id_var;
    } else {
        return push_array_index(p_context, var_id, expr_id);
    }
}

static ketl_hir_var_id_t parse_binary_ltr(ketl_parser_context* p_context, ketl_hir_var_id_t lhs) {
    ketl_token_type token_type = CURRENT_TOKEN(1).type;
    ketl_parse_rule* p_parse_rule = get_parse_rule(token_type);
    ketl_hir_var_id_t rhs = parse_precedence(p_context, p_parse_rule->precedence + 1);

    ANN_SWITCH_STRICT (token_type) {
        case KETL_TOKEN_TYPE_PLUS:             return push_hir_binary_op(p_context, NULL, KETL_HIR_PLUS,             lhs, rhs);
        case KETL_TOKEN_TYPE_MINUS:            return push_hir_binary_op(p_context, NULL, KETL_HIR_MINUS,            lhs, rhs);
        case KETL_TOKEN_TYPE_MULTIPLY:         return push_hir_binary_op(p_context, NULL, KETL_HIR_MULTY,            lhs, rhs);
        case KETL_TOKEN_TYPE_DIVIDE:           return push_hir_binary_op(p_context, NULL, KETL_HIR_DIV,              lhs, rhs);
        case KETL_TOKEN_TYPE_REMAINDER:        return push_hir_binary_op(p_context, NULL, KETL_HIR_MOD,              lhs, rhs);

        case KETL_TOKEN_TYPE_LESS:             return push_hir_binary_op(p_context, NULL, KETL_HIR_LESS,             lhs, rhs);
        case KETL_TOKEN_TYPE_LESS_OR_EQUAL:    return push_hir_binary_op(p_context, NULL, KETL_HIR_LESS_OR_EQUAL,    lhs, rhs);
        case KETL_TOKEN_TYPE_GREATER:          return push_hir_binary_op(p_context, NULL, KETL_HIR_GREATER,          lhs, rhs);
        case KETL_TOKEN_TYPE_GREATER_OR_EQUAL: return push_hir_binary_op(p_context, NULL, KETL_HIR_GREATER_OR_EQUAL, lhs, rhs);
        case KETL_TOKEN_TYPE_EQUAL:            return push_hir_binary_op(p_context, NULL, KETL_HIR_EQUAL,            lhs, rhs);
        case KETL_TOKEN_TYPE_NOT_EQUAL:        return push_hir_binary_op(p_context, NULL, KETL_HIR_NOT_EQUAL,        lhs, rhs);
    }
}

static ketl_hir_var_id_t parse_short_circuit(ketl_parser_context* p_context, ketl_hir_var_id_t lhs) {
    ketl_token_type token_type = CURRENT_TOKEN(1).type;
    ketl_parse_rule* p_parse_rule = get_parse_rule(token_type);

    ketl_hir_block_index_t first_block = reserve_hir_blocks(p_context, 3);
    ketl_hir_block_index_t true_statement = first_block;
    ketl_hir_block_index_t false_statement = first_block + 1;
    ketl_hir_block_index_t after_block = first_block + 2;

    push_hir_if(p_context, NULL, lhs, true_statement, false_statement);

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

    pull_hir_set_block(p_context, second_block);
    ketl_hir_var_id_t rhs = parse_precedence(p_context, p_parse_rule->precedence + 1);
    
    // TODO find common type, set temp var to common type
    ANN_ASSERT(p_context->hir_builder.vars.p_data[lhs].type == p_context->hir_builder.vars.p_data[rhs].type);
    ANN_ASSERT(p_context->hir_builder.vars.p_data[lhs].type != KETL_HIR_USED_TYPE_UNKNOWN);
    ketl_hir_var_id_t output_var = push_temp_var_type(p_context, p_context->hir_builder.vars.p_data[lhs].type); 
    // TODO do casting if necessary
    push_hir_assign_impl(p_context, NULL, output_var, rhs);
    push_hir_jump(p_context, NULL, after_block);
    
    pull_hir_set_block(p_context, short_sircuit_block);
    // TODO do casting if necessary
    push_hir_assign_impl(p_context, NULL, output_var, lhs);
    push_hir_jump(p_context, NULL, after_block);

    pull_hir_set_block(p_context, after_block);

    return output_var;
}

static ketl_hir_var_id_t parse_binary_rtl(ketl_parser_context* p_context, ketl_hir_var_id_t lhs, ketl_hir_var_id_t rhs) {
    ketl_token_type token_type = CURRENT_TOKEN(1).type;

    ANN_SWITCH_STRICT (token_type) {
        case KETL_TOKEN_TYPE_ASSIGN: return push_hir_assign(p_context, NULL, lhs, rhs);
    }
}

ketl_parse_rule parse_rules[] = {
    [KETL_TOKEN_TYPE_ID]                         = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_LITERAL_NULL]               = { parse_null,          NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_LITERAL_INTEGER]            = { parse_number,        NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_LITERAL_STRING]             = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_LITERAL_CHAR]               = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_PARENTHESIS_LEFT]           = { parse_grouping,      parse_call,          NULL,             KETL_PREC_CALL},
    [KETL_TOKEN_TYPE_PARENTHESIS_RIGHT]          = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CURLY_LEFT]                 = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CURLY_RIGHT]                = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_SQUARE_LEFT]                = { NULL,                parse_indexing,      NULL,             KETL_PREC_CALL},
    [KETL_TOKEN_TYPE_SQUARE_RIGHT]               = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DOT]                        = { NULL,                parse_dot_operator,  NULL,             KETL_PREC_CALL},
    [KETL_TOKEN_TYPE_COMMA]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_QUESTION_MARK]              = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_COLON]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ARROW_RIGHT]                = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_TERMINATION_CHARACTER]      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_LOGICAL_NOT]                = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_LOGICAL_AND]                = { NULL,                parse_short_circuit, NULL,             KETL_PREC_LOGICAL_AND},
    [KETL_TOKEN_TYPE_LOGICAL_OR]                 = { NULL,                parse_short_circuit, NULL,             KETL_PREC_LOGICAL_OR},
    [KETL_TOKEN_TYPE_LESS]                       = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_COMPARISON},
    [KETL_TOKEN_TYPE_LESS_OR_EQUAL]              = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_COMPARISON},
    [KETL_TOKEN_TYPE_GREATER]                    = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_COMPARISON},
    [KETL_TOKEN_TYPE_GREATER_OR_EQUAL]           = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_COMPARISON},
    [KETL_TOKEN_TYPE_EQUAL]                      = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_EQUALITY},
    [KETL_TOKEN_TYPE_NOT_EQUAL]                  = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_EQUALITY},
    [KETL_TOKEN_TYPE_BITWISE_NOT]                = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_BITWISE_AND]                = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_BITWISE_OR]                 = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_BITWISE_XOR]                = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_BITWISE_SHIFT_LEFT]         = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_BITWISE_SHIFT_RIGHT]        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_INCREMENT]                  = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DECREMENT]                  = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_PLUS]                       = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_TERM},
    [KETL_TOKEN_TYPE_MINUS]                      = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_TERM},
    [KETL_TOKEN_TYPE_MULTIPLY]                   = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_FACTOR},
    [KETL_TOKEN_TYPE_DIVIDE]                     = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_FACTOR},
    [KETL_TOKEN_TYPE_REMAINDER]                  = { NULL,                parse_binary_ltr,    NULL,             KETL_PREC_FACTOR},
    [KETL_TOKEN_TYPE_ASSIGN]                     = { NULL,                NULL,                parse_binary_rtl, KETL_PREC_ASSIGNMENT},
    [KETL_TOKEN_TYPE_ASSIGN_PLUS]                = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_MINUS]               = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_MULTIPLY]            = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_DIVIDE]              = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_REMAINDER]           = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_BITWISE_SHIFT_LEFT]  = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_BITWISE_SHIFT_RIGHT] = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_BITWISE_AND]         = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_BITWISE_OR]          = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ASSIGN_BITWISE_XOR]         = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_I8]                         = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_I16]                        = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_I32]                        = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_I64]                        = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_U8]                         = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_U16]                        = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_U32]                        = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_U64]                        = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_F32]                        = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_F64]                        = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_BOOL]                       = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_BREAK]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CASE]                       = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CHAR]                       = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_CIMPORT]                    = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CONST]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CONTINUE]                   = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DEFAULT]                    = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DO]                         = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ELSE]                       = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ENUM]                       = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_FALSE]                      = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_FN]                         = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_FOR]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_IF]                         = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_IMPORT]                     = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_NONE]                       = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_RAW]                        = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_RETURN]                     = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_STRUCT]                     = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_SWITCH]                     = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_TRUE]                       = { parse_identificator, NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_UNION]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_VAR]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_WHILE]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_EOF]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ERROR]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
};

static ketl_parse_rule* get_parse_rule(ketl_token_type token_type) {
    return &parse_rules[token_type];
}

static ketl_hir_var_id_t parse_lhs_operand(ketl_parser_context* p_context, ketl_precedence precedence) {    
    ketl_parse_prefix f_prefix_rule = get_parse_rule(CURRENT_TOKEN(1).type)->f_prefix;
    if (f_prefix_rule == NULL) {
        errorf(CURRENT_TOKEN(1).offset, CURRENT_TOKEN(1).length, "Expected expression.");
        return push_temp_var(p_context);
    }

    ketl_hir_var_id_t lhs = f_prefix_rule(p_context);

    while (precedence < get_parse_rule(CURRENT_TOKEN(0).type)->precedence ||
        (associativity[precedence] == KETL_LTR && precedence == get_parse_rule(CURRENT_TOKEN(0).type)->precedence)) {
        token_advance(p_context);
        ketl_parse_ltr_infix f_ltr_infix_rule = get_parse_rule(CURRENT_TOKEN(1).type)->f_ltr_infix;
        if (f_ltr_infix_rule == NULL) {
            errorf(CURRENT_TOKEN(1).offset, CURRENT_TOKEN(1).length, "Expected operator.");
            return push_temp_var(p_context);
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
            ANN_ASSERT(bracket == KETL_PARSER_BRACKET_PARENTHESIS);
            return true;
        case KETL_TOKEN_TYPE_CURLY_RIGHT:
            if (*bracket_stack_count <= 0) {
                return false;
            }
            bracket = p_brackets_stack[--(*bracket_stack_count)];
            ANN_ASSERT(bracket == KETL_PARSER_BRACKET_CURLY);
            return true;
        case KETL_TOKEN_TYPE_SQUARE_RIGHT:
            if (*bracket_stack_count <= 0) {
                return false;
            }
            bracket = p_brackets_stack[--(*bracket_stack_count)];
            ANN_ASSERT(bracket == KETL_PARSER_BRACKET_SQUARE);
            return true;
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
        p_parse_rule = get_parse_rule(CURRENT_TOKEN(1).type);
        if (!dummy_parse_bracket(CURRENT_TOKEN(1).type, a_brackets_stack, &bracket_stack_count)) {
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
    ANN_ASSERT(f_rtl_infix != NULL);
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

static ketl_statement_info parse_block_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // {
    ketl_statement_info statement_info = parse_block_statement_inner(p_context);
    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT, "Expected '}' at the end of the block.");
    return statement_info;
}

static ketl_statement_info parse_if_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // if

    ketl_hir_block_index_t first_block = reserve_hir_blocks(p_context, 2);
    ketl_hir_block_index_t true_statement = first_block;
    ketl_hir_block_index_t false_statement = first_block + 1;

    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_LEFT, "Expected '(' after if keyword.");
    ketl_hir_var_id_t expr = parse_expression(p_context);
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after if statement expression.");

    push_hir_if(p_context, NULL, expr, true_statement, false_statement);

    pull_hir_set_block(p_context, true_statement);
    ketl_parse_return_info true_return_info = parse_statement(p_context).return_info;

    ketl_parse_return_info return_info;

    if (token_match(p_context, KETL_TOKEN_TYPE_ELSE)) {
        if (true_return_info & KETL_RETURN_ALWAYS) {
            pull_hir_set_block(p_context, false_statement);
            ketl_parse_return_info false_return_info = parse_statement(p_context).return_info;
            if (!(false_return_info & KETL_RETURN_ALWAYS)) {
                ketl_hir_block_index_t after_block = reserve_hir_blocks(p_context, 1);
                push_hir_jump(p_context, NULL, after_block);
                pull_hir_set_block(p_context, after_block);
            }

            return_info = 
                ((true_return_info & false_return_info) & KETL_RETURN_ALWAYS) |
                ((true_return_info | false_return_info) & KETL_RETURN_UNDEF);
        } else {
            ketl_hir_block_index_t after_block = reserve_hir_blocks(p_context, 1);
            push_hir_jump(p_context, NULL, after_block);
        
            pull_hir_set_block(p_context, false_statement);
            ketl_parse_return_info false_return_info = parse_statement(p_context).return_info;
            if (!(false_return_info & KETL_RETURN_ALWAYS)) {
                push_hir_jump(p_context, NULL, after_block);
            }

            pull_hir_set_block(p_context, after_block);

            return_info = 
                ((true_return_info & false_return_info) & KETL_RETURN_ALWAYS) |
                ((true_return_info | false_return_info) & KETL_RETURN_UNDEF);
        }
    } else {
        if (!(true_return_info & KETL_RETURN_ALWAYS)) {
            push_hir_jump(p_context, NULL, false_statement);
        }
        pull_hir_set_block(p_context, false_statement);

        return_info = true_return_info & KETL_RETURN_UNDEF;
    }

    return (ketl_statement_info){ .return_info = return_info };
}

static ketl_statement_info parse_return_statement(ketl_parser_context* p_context) {
    token_advance(p_context); // return
    if (token_match(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER)) {
        push_hir_instr(p_context, NULL, KETL_HIR_RETURN);
        return (ketl_statement_info){ .return_info = KETL_RETURN_ALWAYS_NONE };
    } else {
        push_hir_return_value(p_context, NULL, parse_expression(p_context));
        token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' after return statement expression.");
        return (ketl_statement_info){ .return_info = KETL_RETURN_ALWAYS_VALUE };
    }
}

static ketl_statement_info parse_statement(ketl_parser_context* p_context) {
    switch (CURRENT_TOKEN(0).type) {
        case KETL_TOKEN_TYPE_CURLY_LEFT           : return parse_block_statement     (p_context); break;
        case KETL_TOKEN_TYPE_IF                   : return parse_if_statement        (p_context); break;
        case KETL_TOKEN_TYPE_RETURN               : return parse_return_statement    (p_context); break;
        case KETL_TOKEN_TYPE_TERMINATION_CHARACTER:        token_advance             (p_context); 
                                                    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
        default                                   : return parse_expression_statement(p_context); break;
    }
}

static ketl_hir_used_type_index_t parse_type(ketl_parser_context* p_context) {
    ketl_token_t token = CURRENT_TOKEN(0);
    ketl_hir_used_type_index_t type = find_type(p_context, token);
    if (type == KETL_HIR_USED_TYPE_UNKNOWN) {
        errorf(token.offset, token.length, "'%.*s' is not a known type.", TOKEN_LENGTH(token), TOKEN_STRING(token));
    }
    token_advance(p_context);
    return type;
}

static ketl_type* index_to_type(ketl_parser_context* p_context, ketl_hir_used_type_index_t type) {
    return type != KETL_HIR_USED_TYPE_UNKNOWN ? p_context->hir_builder.used_types.p_data[type] : NULL; 
}

static ketl_statement_info parse_import(ketl_parser_context* p_context) {
    token_advance(p_context); // var
    ketl_token_t module_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // id

    const char* p_module_name = TOKEN_STRING(module_literal);
    uint32_t module_name_length = TOKEN_LENGTH(module_literal);

    ketl_atomic_string s_module_name = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, p_module_name, module_name_length);

    ketl_state_load_module_impl(p_context->p_state, s_module_name, p_context->p_namespace);

    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_var_declaration(ketl_parser_context* p_context) {
    uint32_t decl_start = CURRENT_TOKEN(0).offset;

    token_advance(p_context); // var
    ketl_token_t id_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // id
    ketl_hir_used_type_index_t type = KETL_HIR_USED_TYPE_UNKNOWN;
    ketl_hir_var_id_t initial_value = -1;
    if (token_match(p_context, KETL_TOKEN_TYPE_COLON)) {
        type = parse_type(p_context);
    }
    if (token_match(p_context, KETL_TOKEN_TYPE_ASSIGN)) {
        initial_value = parse_expression(p_context);
        
        // TODO conversion!!!;
        if (type == KETL_HIR_USED_TYPE_UNKNOWN) {
            type = p_context->hir_builder.vars.p_data[initial_value].type;
        }
    } else {
        if (type == KETL_HIR_USED_TYPE_UNKNOWN) {
            uint32_t decl_end = CURRENT_TOKEN(1).offset + CURRENT_TOKEN(1).length;
            errorf(decl_start, decl_end - decl_start, "Variable declaration without a type expects an initial expression.");

            initial_value = push_temp_var(p_context);
        } else {
            // TODO do default contructor or smth
            ketl_type* p_type = p_context->hir_builder.used_types.p_data[type];
            if (p_type->kind == KETL_TYPE_PRIMITIVE) {
                initial_value = push_literal_number_symbol_of_type(p_context, push_symbol_string(p_context, "0", 1), type);
            } else {
                initial_value = push_literal_number_symbol_of_type(p_context, KETL_HIR_LITERAL_NULL, type);
            }
            
            type = p_context->hir_builder.vars.p_data[initial_value].type;
        }
    }
    ANN_ASSERT(type != KETL_HIR_USED_TYPE_UNKNOWN);
    push_hir_variable_declaration(p_context, NULL, id_literal, type, initial_value);
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

            a_function_parameters_named[function_parameters.parameters_count].info.p_type = index_to_type(p_context, parse_type(p_context));
            a_function_parameters[function_parameters.parameters_count + 1] = a_function_parameters_named[function_parameters.parameters_count].info;

            ++function_parameters.parameters_count;
        } while (token_match(p_context, KETL_TOKEN_TYPE_COMMA));
    }
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after parameters.");

    ketl_type* return_type = NULL;
    if (token_match(p_context, KETL_TOKEN_TYPE_ARROW_RIGHT)) {
        return_type = p_context->hir_builder.used_types.p_data[parse_type(p_context)];
    }
    if (return_type == NULL) {
        return_type = ketl_state_get_none_type(p_context->p_state);
    }

    a_function_parameters[0].p_type = return_type;
    ++function_parameters.parameters_count;

    ketl_type* function_type = ketl_state_get_cfunction_type(p_context->p_state, &function_parameters);
    ketl_namespace_node* p_func_node = ketl_state_define_function(p_context->p_state, p_context->p_namespace, TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal), function_type, NULL);
    // TODO fix
    // export will change asm name of the function
    // cimport should use asm name as is, but still optionally 'export'able
    p_func_node->export = true;

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

            a_function_parameters_named[function_parameters.parameters_count].info.p_type = index_to_type(p_context, parse_type(p_context));
            a_function_parameters[function_parameters.parameters_count + 1] = a_function_parameters_named[function_parameters.parameters_count].info;

            ++function_parameters.parameters_count;
        } while (token_match(p_context, KETL_TOKEN_TYPE_COMMA));
    }
    token_consume(p_context, KETL_TOKEN_TYPE_PARENTHESIS_RIGHT, "Expected ')' after parameters.");

    ketl_type* return_type = NULL;
    if (token_match(p_context, KETL_TOKEN_TYPE_ARROW_RIGHT)) {
        return_type = p_context->hir_builder.used_types.p_data[parse_type(p_context)];
    }
    if (return_type == NULL) {
        return_type = ketl_state_get_none_type(p_context->p_state);
    }

    uint32_t parameters_count = function_parameters.parameters_count;
    a_function_parameters[0].p_type = return_type;
    ++function_parameters.parameters_count;
    
    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_LEFT, "Expected '{' after function declaration.");

    ketl_type* function_type = ketl_state_get_function_type(p_context->p_state, &function_parameters);
    ketl_namespace_node* p_func_node = ketl_state_define_function(p_context->p_state, p_context->p_namespace, TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal), function_type, NULL);
    p_func_node->export = p_context->export;
    ketl_atomic_string s_func_name = ketl_atomic_strings_get(&p_context->p_state->atomic_strings, TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal));
    
    // looking for the end of the function definition
    ketl_token_iterator_t start_pos = p_context->p_lexer->token_iterator;
    ketl_parser_bracket_t a_brackets_stack[32];
    uint32_t bracket_stack_count = 0;

    dummy_parse_bracket(KETL_TOKEN_TYPE_CURLY_LEFT, a_brackets_stack, &bracket_stack_count);
    while (bracket_stack_count > 0) {
        dummy_parse_bracket(CURRENT_TOKEN(0).type, a_brackets_stack, &bracket_stack_count);
        token_advance(p_context);
    }

    // TODO do proper error evaluation during this pass
    //token_consume(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT, "Expected '}' after function body.");

    // saving end of the function definition for later
    ketl_token_iterator_t end_pos = p_context->p_lexer->token_iterator;

    compile_function_declaration_t function_decl = {
        .s_name = s_func_name,
        .p_namespace_node = p_func_node,
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

    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_class_declaration(ketl_parser_context* p_context) {
    token_advance(p_context); // class
    ketl_token_t id_literal = CURRENT_TOKEN(0);
    token_advance(p_context); // id

    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_LEFT, "Expected '{' after class name.");

    ketl_named_variable_type_info_t a_class_fields[256] = {0};
    uint32_t class_field_count = 0;

    if (!token_check(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT)) {
        do {
            ketl_token_t field_literal = CURRENT_TOKEN(0);
            a_class_fields[class_field_count].p_name = TOKEN_STRING(field_literal);
            a_class_fields[class_field_count].name_length = TOKEN_LENGTH(field_literal);
            token_advance(p_context); // id

            token_consume(p_context, KETL_TOKEN_TYPE_COLON, "Expected ':' after field name.");

            a_class_fields[class_field_count].info.p_type = index_to_type(p_context, parse_type(p_context));

            token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' after field declaration.");

            ++class_field_count;
        } while (token_check(p_context, KETL_TOKEN_TYPE_ID));
    }
    
    ketl_namespace_node* p_class_node = ketl_state_define_class(p_context->p_state, p_context->p_namespace, TOKEN_STRING(id_literal), TOKEN_LENGTH(id_literal), a_class_fields, class_field_count);
    p_class_node->export = true;
    token_consume(p_context, KETL_TOKEN_TYPE_CURLY_RIGHT, "Expected '}' in the end of a class declaration.");
    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_export_declaration(ketl_parser_context* p_context) {
    ketl_token_t literal = CURRENT_TOKEN(0);
    token_advance(p_context);
    p_context->export = true;

    ketl_statement_info info;
    switch (CURRENT_TOKEN(0).type) {
        case KETL_TOKEN_TYPE_IMPORT : info = parse_import              (p_context); break;
        case KETL_TOKEN_TYPE_CIMPORT: info = parse_cimport_declaration (p_context); break;
        case KETL_TOKEN_TYPE_VAR    : info = parse_var_declaration     (p_context); break;
        case KETL_TOKEN_TYPE_FN     : info = parse_function_declaration(p_context); break;
        case KETL_TOKEN_TYPE_CLASS  : info = parse_class_declaration   (p_context); break;
        case KETL_TOKEN_TYPE_EXPORT : 
            // TODO do warning instead
            errorf(literal.offset, literal.length, "Redundant 'export' keyword.");
            return parse_export_declaration(p_context);
        default:
            // TODO do warning instead?
            errorf(literal.offset, literal.length, "Expected declaration after 'export'.");
            p_context->export = false;
            return parse_declaration(p_context);
    }
    p_context->export = false;
    return info;
}

static ketl_statement_info parse_declaration(ketl_parser_context* p_context) {
    switch (CURRENT_TOKEN(0).type) {
        case KETL_TOKEN_TYPE_IMPORT : return parse_import              (p_context); break;
        case KETL_TOKEN_TYPE_CIMPORT: return parse_cimport_declaration (p_context); break;
        case KETL_TOKEN_TYPE_VAR    : return parse_var_declaration     (p_context); break;
        case KETL_TOKEN_TYPE_FN     : return parse_function_declaration(p_context); break;
        case KETL_TOKEN_TYPE_CLASS  : return parse_class_declaration   (p_context); break;
        case KETL_TOKEN_TYPE_EXPORT : return parse_export_declaration  (p_context); break;
        default                     : return parse_statement           (p_context); break;
    }
}

void ketl_parser_build_hir(ketl_state* p_state, ketl_hir_t* p_hir, ketl_lexer_t* p_lexer, ketl_token_iterator_t end_pos, ketl_namespace* p_namespace, ketl_named_variable_type_info_t* p_parameters, uint32_t parameter_count, bool vars_in_namespace, const ketl_allocator* p_allocator) {
    *p_hir = (ketl_hir_t){0};

    ketl_parser_context context = {0};
    context = (ketl_parser_context){
        .p_state = p_state, 
        .p_lexer = p_lexer,
        .p_namespace = p_namespace,
        .end_pos = end_pos,
        .vars_in_namespace = vars_in_namespace,
    };
    ketl_parser_context* p_context = &context;

    ketl_hir_builder_init(&context.hir_builder, p_state, p_lexer, p_allocator);
    context.s_filename = push_symbol_string(&context, 
        ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_lexer->s_filename), KETL_NULL_TERMINATED_LENGTH_32);

    for (uint32_t i = 0u; i < parameter_count; ++i) {
        ketl_hir_builder_add_parameter(&context.hir_builder, p_namespace, &p_parameters[i]);
    }
    
    if (p_lexer->tokens.size > 0) {
        _ketl_parse_argument_stack_t_init(&context.argument_stack, 4, p_allocator);

        ketl_statement_info statement_info = parse_block_statement_inner(&context);
        if ((statement_info.return_info & KETL_RETURN_UNDEF) == KETL_RETURN_UNDEF ||
            (!(statement_info.return_info & KETL_RETURN_ALWAYS) && (statement_info.return_info & KETL_RETURN_ALWAYS_VALUE))) {
            // TODO POS?
            errorf(0, 1, "Not all control returns value.");
        }

        if (!(statement_info.return_info & KETL_RETURN_ALWAYS)) {
            // we need to return eventually
            push_hir_instr(p_context, NULL, KETL_HIR_RETURN);
        }

        _ketl_parse_argument_stack_t_deinit(&context.argument_stack);
    }

    ketl_hir_builder_flush(&context.hir_builder, p_hir);

    return;
}
