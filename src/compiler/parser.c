//🫖ketl
#include "compiler/parser.h"

#include "ketl_impl.h"

#include "compiler/lexer.h"

#include "containers/vector.h"
#include "containers/hash_map.h"
#include "memory_impl.h"
#include "str.h"

#include <stdio.h>
#include <stdarg.h>

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
    ketl_token_iterator_t token_iterator;
    ketl_hir_symbol_offset_t a_filename;
    ketl_hir_builder_t hir_builder;

    _ketl_parse_argument_stack_t v_argument_stack;
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

#define EOF_STR "EOF"
#define TOKEN_LENGTH(token) ((int)((token).length > 0 ? (token).length : sizeof(EOF_STR) - 1))
#define TOKEN_STRING(token) ((token).length > 0 ? (p_context)->p_lexer->p_source + (token).offset : EOF_STR)

#define TOKEN(index) ((p_context)->p_lexer->v_tokens.p_data[(index)])
#define CURRENT_TOKEN(offset) (TOKEN((p_context)->token_iterator - (offset)))

#define GET_SYMBOL_SOURCE(symbol_offset) (ketl_atomic_strings_get_pointer(&p_context->hir_builder.symbols, (symbol_offset)))

char error_buffer[256];

ANN_DEFINE(ketl_error_info) {
    ketl_lexer_t* p_lexer;
    const char* p_filename;
    uint32_t offset;
    uint32_t length;
};

static void report_error(string_builder_t* p_error_stream, ketl_error_info* p_error_info, const char* format, ...) {
    uint32_t line, col;
    line = ketl_lexer_find_line(p_error_info->p_lexer, p_error_info->offset);
    col = p_error_info->offset - ketl_lexer_get_line_offset(p_error_info->p_lexer, line);

    int message_size = 0;
    message_size += snprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, 
        "%s:%"PRIu32":%"PRIu32": error: ", p_error_info->p_filename, line + 1, col + 1);

    va_list vargs;
    va_start(vargs, format);
    message_size += vsnprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, format, vargs);
    va_end(vargs);

    uint32_t line_start = ketl_lexer_get_line_offset(p_error_info->p_lexer, line);
    uint32_t line_end = ketl_lexer_get_line_offset(p_error_info->p_lexer, line + 1);
    // TODO check for all new line configs
    if (line_end > 0 && p_error_info->p_lexer->p_source[line_end - 1] == '\n') {
        --line_end;
    }
    message_size += snprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, 
        "\n %4d |%.*s", line + 1, line_end - line_start, p_error_info->p_lexer->p_source + line_start);

    message_size += snprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, 
        "\n      |%*s^", col, "");
    for (uint32_t i = 1; i < p_error_info->length; ++i) {
        message_size += snprintf(error_buffer + message_size, ANN_ARRAY_SIZE(error_buffer) - message_size, 
        "~");
    }

    string_builder_t_push_back_ref_n(p_error_stream, error_buffer, message_size);
    string_builder_t_push_back_copy(p_error_stream, '\n');
}

#define error(__offset, __length, message) \
do {\
    ketl_error_info error_info = {\
        .p_lexer = p_context->p_lexer,\
        .p_filename = GET_SYMBOL_SOURCE(p_context->a_filename),\
        .offset = (__offset),\
        .length = (__length),\
    };\
    report_error(&p_context->p_state->error_stream, &error_info, message);\
} while (0)
#define errorf(__offset, __length, format, ...) \
do {\
    ketl_error_info error_info = {\
        .p_lexer = p_context->p_lexer,\
        .p_filename = GET_SYMBOL_SOURCE(p_context->a_filename),\
        .offset = (__offset),\
        .length = (__length),\
    };\
    report_error(&p_context->p_state->error_stream, &error_info, format, __VA_ARGS__);\
} while (0)

static ketl_hir_symbol_offset_t push_symbol_string(ketl_parser_context* p_context, const char* p_str, uint32_t length) {
    return (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_context->hir_builder.symbols, p_str, length);
}

static ketl_hir_symbol_offset_t push_symbol(ketl_parser_context* p_context, ketl_token_t literal) {
    return push_symbol_string(p_context, TOKEN_STRING(literal), TOKEN_LENGTH(literal));
}

static ketl_hir_var_id_t push_literal_number_symbol(ketl_parser_context* p_context, ketl_hir_symbol_offset_t literal) {
    // TODO determine correct type
    ketl_type* p_type = ketl_state_get_i64(p_context->p_state);
    ketl_hir_used_type_index_t type = ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_type);
    ketl_hir_var_id_t literal_var = ketl_hir_builder_get_literal(&p_context->hir_builder, literal, type);
    return literal_var;
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
        .file_symbol = p_context->a_filename,
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
    ketl_hir_builder_insert_binary_op(p_context->p_state, &p_context->hir_builder, header, &instr);
    return output_var;
}

static void push_hir_assign_impl(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t lhs_var, ketl_hir_var_id_t rhs_var) {

    ketl_hir_var_t* p_lhs_var = p_context->hir_builder.v_vars.p_data + lhs_var;
    ketl_hir_var_t* p_rhs_var = p_context->hir_builder.v_vars.p_data + rhs_var;

    if (p_lhs_var->uid == KETL_HIR_VAR_UID_LITERAL) {
        ANN_ASSERT(false && "Can't assign to an r-value.");
    }
    
    // TODO casting if needed

    if (p_rhs_var->uid != KETL_HIR_VAR_UID_LITERAL && p_rhs_var->info == KETL_HIR_VAR_INFO_TEMP) {
        ketl_hir_builder_replace_temp_var(&p_context->hir_builder, lhs_var, rhs_var);
        return;
    }

    (void)p_pos_info;
    ketl_hir_header_t assign_header = {
        // TODO fix size
        .tag = KETL_HIR_ASSIGN | KETL_HIR_I64,
        .file_symbol = p_context->a_filename,
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
    ketl_hir_var_t* p_lhs_var = p_context->hir_builder.v_vars.p_data + lhs_var;
    if (p_lhs_var->uid == KETL_HIR_VAR_UID_LITERAL) {
        // TODO POS
        error(0, 1, "Can't assign to an l-value.");
        return push_temp_var(p_context);
    }

    if (p_lhs_var->info != KETL_HIR_VAR_INFO_TEMP &&
        p_context->hir_builder.v_vars_infos.p_data[p_lhs_var->info].p_global == NULL) {
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
        .file_symbol = p_context->a_filename,
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

static uint16_t push_literal_id(ketl_parser_context* p_context, ketl_token_t literal) {
    // TODO decide how to update uids
    ketl_hir_var_id_t id_var = ketl_hir_builder_get_var(p_context->p_state, &p_context->hir_builder, 
        push_symbol(p_context, literal), KETL_HIR_USED_TYPE_UNKNOWN);
    if (id_var == (ketl_hir_var_id_t)-1) {
        errorf(literal.offset, literal.length, "Use of undeclared variable '%.*s'.", TOKEN_LENGTH(literal), TOKEN_STRING(literal));
        id_var = push_temp_var(p_context);
    }
    return id_var;
}

static ketl_hir_used_type_index_t find_type(ketl_parser_context* p_context, ketl_token_t type_literal) {
    ketl_type* p_type = ketl_state_get_type(p_context->p_state, TOKEN_STRING(type_literal), TOKEN_LENGTH(type_literal));
    return ketl_hir_builder_get_used_type_index(&p_context->hir_builder, p_type);
}

static void push_hir_variable_declaration(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_token_t id_literal, ketl_hir_used_type_index_t type_index, ketl_hir_var_id_t init_var) {
    ketl_hir_var_id_t id_var = ketl_hir_builder_register_var(&p_context->hir_builder, 
        push_symbol(p_context, id_literal), type_index);

    push_hir_assign_impl(p_context, p_pos_info, id_var, init_var);
}

static void push_hir_argument(ketl_parser_context* p_context, ketl_hir_var_id_t var_id) {
    _ketl_parse_argument_stack_t_push_back_copy(&p_context->v_argument_stack, var_id);
}

static ketl_hir_var_id_t push_hir_call(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t callee_id, uint16_t arguments_count) {
    (void)p_pos_info;
    ketl_hir_header_t header = {
        .tag = KETL_HIR_CALL,
        .file_symbol = p_context->a_filename,
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

    ketl_hir_builder_insert_call(p_context->p_state, &p_context->hir_builder, header, &instr,
        // pass pointer to last 'arguments_count' elements and immidiatly cut 'arguments_count' tail
        p_context->v_argument_stack.p_data + (p_context->v_argument_stack.size -= arguments_count));
    return output_var;
}

static ketl_hir_block_index_t reserve_hir_blocks(ketl_parser_context* p_context, uint8_t count) {
    ketl_hir_block_index_t first_block = (ketl_hir_block_index_t)p_context->hir_builder.v_blocks.size;
    hir_builder_blocks_t_reserve(&p_context->hir_builder.v_blocks, p_context->hir_builder.v_blocks.size + count);
    for (uint8_t i = count; i > 0; --i) {
        hir_builder_blocks_t_push_back_copy(&p_context->hir_builder.v_blocks, 0u);
    }
    return first_block;
}
static void pull_hir_set_block(ketl_parser_context* p_context, ketl_hir_block_index_t block) {
    p_context->hir_builder.v_blocks.p_data[block] = p_context->hir_builder.v_instrs.size;
    hir_builder_offset_to_block_t_get_or_insert_copy(&p_context->hir_builder.m_offset_to_block, p_context->hir_builder.v_instrs.size, block);
}

static void push_hir_if(ketl_parser_context* p_context, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t bool_expr_var, ketl_hir_block_index_t true_statement, ketl_hir_block_index_t false_statement) {
    // TODO token_check casting
    
    (void)p_pos_info;
    ketl_hir_header_t if_header = {
        .tag = KETL_HIR_JUMP_IF_TRUE,
        .file_symbol = p_context->a_filename,
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
        .file_symbol = p_context->a_filename,
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
        .file_symbol = p_context->a_filename,
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
        uint32_t current = ++p_context->token_iterator;

        if (TOKEN(current).type != KETL_TOKEN_TYPE_ERROR) break;

        // TODO move error to the lexer, make it informative
        error(TOKEN(current).offset, TOKEN(current).length, "Error in lexer.");
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
    return push_hir_call(p_context, NULL, callee, argument_count);
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
    ANN_ASSERT(p_context->hir_builder.v_vars.p_data[lhs].type == p_context->hir_builder.v_vars.p_data[rhs].type);
    ANN_ASSERT(p_context->hir_builder.v_vars.p_data[lhs].type != KETL_HIR_USED_TYPE_UNKNOWN);
    ketl_hir_var_id_t output_var = push_temp_var_type(p_context, p_context->hir_builder.v_vars.p_data[lhs].type); 
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
    [KETL_TOKEN_TYPE_ID]                         = { parse_identificator, NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_LITERAL_INTEGER]            = { parse_number,        NULL,                NULL,             KETL_PREC_PRIMARY},
    [KETL_TOKEN_TYPE_LITERAL_STRING]             = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_LITERAL_CHAR]               = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_PARENTHESIS_LEFT]           = { parse_grouping,      parse_call,          NULL,             KETL_PREC_CALL},
    [KETL_TOKEN_TYPE_PARENTHESIS_RIGHT]          = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CURLY_LEFT]                 = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CURLY_RIGHT]                = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_SQUARE_LEFT]                = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_SQUARE_RIGHT]               = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DOT]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_COMMA]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_QUESTION_MARK]              = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_COLON]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
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
    [KETL_TOKEN_TYPE_I8]                         = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_I16]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_I32]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_I64]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_U8]                         = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_U16]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_U32]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_U64]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_F32]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_F64]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_BOOL]                       = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_BREAK]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CASE]                       = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CHAR]                       = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CONST]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_CONTINUE]                   = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DEFAULT]                    = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_DO]                         = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ELSE]                       = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_ENUM]                       = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_FALSE]                      = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_FOR]                        = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_IF]                         = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_NONE]                       = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_RETURN]                     = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_STRUCT]                     = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_SWITCH]                     = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
    [KETL_TOKEN_TYPE_TRUE]                       = { NULL,                NULL,                NULL,             KETL_PREC_NONE},
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
        error(CURRENT_TOKEN(1).offset, CURRENT_TOKEN(1).length, "Expected expression.");
        return push_temp_var(p_context);
    }

    ketl_hir_var_id_t lhs = f_prefix_rule(p_context);

    while (precedence < get_parse_rule(CURRENT_TOKEN(0).type)->precedence ||
        (associativity[precedence] == KETL_LTR && precedence == get_parse_rule(CURRENT_TOKEN(0).type)->precedence)) {
        token_advance(p_context);
        ketl_parse_ltr_infix f_ltr_infix_rule = get_parse_rule(CURRENT_TOKEN(1).type)->f_ltr_infix;
        if (f_ltr_infix_rule == NULL) {
            error(CURRENT_TOKEN(1).offset, CURRENT_TOKEN(1).length, "Expected operator.");
            return push_temp_var(p_context);
        }
        lhs = f_ltr_infix_rule(p_context, lhs);
    }
   
    return lhs;
}

static ketl_hir_var_id_t parse_precedence(ketl_parser_context* p_context, ketl_precedence precedence) {
    token_advance(p_context);

    if (associativity[precedence] == KETL_LTR) {
        ketl_hir_var_id_t lhs = parse_lhs_operand(p_context, precedence);
        return lhs;
    }

    // looking for the rtl operator
    ketl_token_iterator_t start_pos = p_context->token_iterator;
    ketl_parse_rule* p_parse_rule = get_parse_rule(CURRENT_TOKEN(1).type);
    ketl_precedence token_precedence = p_parse_rule->precedence;
    while (precedence < token_precedence) {
        token_advance(p_context);
        p_parse_rule = get_parse_rule(CURRENT_TOKEN(1).type);
        token_precedence = p_parse_rule->precedence;
    }

    // we coudln't find rtl operator, restoring token iterator and do simple ltr
    if (precedence != token_precedence) {
        p_context->token_iterator = start_pos;
        
        ketl_hir_var_id_t lhs = parse_lhs_operand(p_context, precedence);
        return lhs;
    }

    // parse after the operator, rhs operand
    ketl_hir_var_id_t rhs = parse_precedence(p_context, precedence);

    // saving end of the rhs operand for restoring later
    ketl_token_iterator_t end_pos = p_context->token_iterator;
    p_context->token_iterator = start_pos;

    ketl_hir_var_id_t lhs = parse_lhs_operand(p_context, precedence);
        
    token_advance(p_context);

    // back to the rtl operator
    ketl_parse_rtl_infix f_rtl_infix = get_parse_rule(CURRENT_TOKEN(1).type)->f_rtl_infix;
    ANN_ASSERT(f_rtl_infix != NULL);
    lhs = f_rtl_infix(p_context, lhs, rhs);

    // restore token iterator
    p_context->token_iterator = end_pos;
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

            error(statement_start, statement_end - statement_start, "Unreachable statement.");

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
    ketl_hir_used_type_index_t type = find_type(p_context, CURRENT_TOKEN(0));
    token_advance(p_context);
    return type;
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
            type = p_context->hir_builder.v_vars.p_data[initial_value].type;
        }
    } else {
        if (type == KETL_HIR_USED_TYPE_UNKNOWN) {
            uint32_t decl_end = CURRENT_TOKEN(1).offset + CURRENT_TOKEN(1).length;
            error(decl_start, decl_end - decl_start, "Variable declaration without a type expects an initial expression.");

            initial_value = push_temp_var(p_context);
        } else {
            // TODO do default contructor or smth
            initial_value = push_literal_number_symbol(p_context, push_symbol_string(p_context, "0", 1));
            
            type = p_context->hir_builder.v_vars.p_data[initial_value].type;
        }
    }
    push_hir_variable_declaration(p_context, NULL, id_literal, type, initial_value);
    token_consume(p_context, KETL_TOKEN_TYPE_TERMINATION_CHARACTER, "Expected ';' after variable parse_declaration.");
    return (ketl_statement_info){ .return_info = KETL_RETURN_EMPTY };
}

static ketl_statement_info parse_declaration(ketl_parser_context* p_context) {
    switch (CURRENT_TOKEN(0).type) {
        case KETL_TOKEN_TYPE_VAR: return parse_var_declaration(p_context); break;
        default                 : return parse_statement      (p_context); break;
    }
}

void ketl_parser_build_hir(ketl_state* p_state, ketl_hir_t* p_hir, const char* p_filename, ketl_lexer_t* p_lexer, const ketl_allocator* p_allocator) {
    *p_hir = (ketl_hir_t){0};

    ketl_parser_context context = {0};
    ketl_parser_context* p_context = &context;
    context = (ketl_parser_context){
        .p_state = p_state, 
        .p_lexer = p_lexer
    };
    ketl_hir_builder_init(&context.hir_builder, p_allocator);
    context.a_filename = push_symbol_string(&context, p_filename, KETL_NULL_TERMINATED_LENGTH_32);
    context.token_iterator = (ketl_token_iterator_t)-1;
    
    if (p_lexer->v_tokens.size > 0) {
        _ketl_parse_argument_stack_t_init(&context.v_argument_stack, 4, p_allocator);

        token_advance(&context);
        ketl_statement_info statement_info = parse_block_statement_inner(&context);
        if ((statement_info.return_info & KETL_RETURN_UNDEF) == KETL_RETURN_UNDEF ||
            (!(statement_info.return_info & KETL_RETURN_ALWAYS) && (statement_info.return_info & KETL_RETURN_ALWAYS_VALUE))) {
            // TODO POS?
            error(0, 1, "Not all control returns value.");
        }

        if (!(statement_info.return_info & KETL_RETURN_ALWAYS)) {
            // we need to return eventually
            push_hir_instr(p_context, NULL, KETL_HIR_RETURN);
        }

        _ketl_parse_argument_stack_t_deinit(&context.v_argument_stack);
    }

    ketl_hir_builder_flush(p_state, &context.hir_builder, p_hir);

    return;
}
