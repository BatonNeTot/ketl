//🫖ketl
#include "compiler/parser.h"

#include "ketl_impl.h"

#include "compiler/lexer.h"

#include "containers/vector.h"
#include "containers/hash_map.h"
#include "memory_impl.h"
#include "str.h"

#include <stdio.h>

KETL_DEFINE(ketl_parse_node_token) {
    ketl_token body;
    uint32_t offset;
};

typedef union {
    ketl_hir_var_id_t var_id;
    ketl_hir_used_type_index_t type_index;
    uint16_t arguments_counter;
    ketl_parse_node_token token;
} ketl_parse_node_output;

KETL_DEFINE(ketl_parse_pos_info) {
    uint32_t start_pos_line;
    uint32_t end_pos_line;
    uint16_t start_pos_col;
    uint16_t end_pos_col;
};

typedef uint16_t lalr_state;

KETL_DEFINE(ketl_parse_node) {
    lalr_state state;
    ketl_parse_pos_info pos_info;
    ketl_parse_node_output output;
};

KETL_VECTOR_DECLARATION(_ketl_parse_node_stack_t, ketl_parse_node)
KETL_VECTOR_DEFINITION(_ketl_parse_node_stack_t, ketl_parse_node)

KETL_VECTOR_DECLARATION(_ketl_parse_argument_stack_t, ketl_hir_var_id_t)
KETL_VECTOR_DEFINITION(_ketl_parse_argument_stack_t, ketl_hir_var_id_t)

KETL_VECTOR_DECLARATION(_ketl_parse_flow_stack_t, ketl_hir_block_index_t)
KETL_VECTOR_DEFINITION(_ketl_parse_flow_stack_t, ketl_hir_block_index_t)

KETL_DEFINE(ketl_parser_context) {
    ketl_state* p_state;
    const char* p_source;
    uint32_t offset;
    ketl_hir_symbol_offset_t p_filename;
    ketl_hir_block_index_t next_block;
    ketl_hir_builder_t hir_builder;
    _ketl_parse_node_stack_t v_node_stack;

    _ketl_parse_argument_stack_t v_argument_stack;
    _ketl_parse_flow_stack_t v_flow_stack;
};

#define stack_top_node(index) (pContext->v_node_stack.pData[pContext->v_node_stack.size - (index)])
#define stack_top(index) (stack_top_node(index).output)
#define node_token_source(ketl_parse_node_output) (pContext->p_source + (ketl_parse_node_output).token.offset)

static uint16_t push_symbol(ketl_parser_context* pContext, ketl_parse_node_output literal) {
    return (uint16_t)ketl_atomic_strings_get(&pContext->hir_builder.symbols, node_token_source(literal), literal.token.body.length);
}

static uint16_t push_literal_id(ketl_parser_context* pContext, ketl_parse_node_output literal) {
    // TODO decide how to update uids
    ketl_hir_var_id_t id_var = ketl_hir_builder_get_var(pContext->p_state, &pContext->hir_builder, 
        push_symbol(pContext, literal), KETL_HIR_USED_TYPE_UNKHOWN);
    return id_var;
}

static uint16_t push_literal_number(ketl_parser_context* pContext, ketl_parse_node_output literal) {
    // TODO determine correct type
    ketl_type* p_type = ketl_state_get_i64(pContext->p_state);
    ketl_hir_used_type_index_t type = ketl_hir_builder_get_used_type_index(&pContext->hir_builder, p_type);
    ketl_hir_var_id_t literal_var = ketl_hir_builder_get_literal(&pContext->hir_builder, 
        push_symbol(pContext, literal), type);
    return literal_var;
}

static ketl_hir_var_id_t push_temp_var(ketl_parser_context* pContext) {
    return ketl_hir_builder_create_temp_var(&pContext->hir_builder, KETL_HIR_USED_TYPE_UNKHOWN);
}

static ketl_hir_var_id_t push_hir_binary_op(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_hir_tag_t hir_tag, ketl_hir_var_id_t lhs, ketl_hir_var_id_t rhs) {
    ketl_hir_header_t header = {
        .tag = hir_tag,
        .file_symbol = pContext->p_filename,
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
    };
    ketl_hir_var_id_t output_var = push_temp_var(pContext); 
    ketl_hir_binary_op_t instr = {
        .output_var = output_var,
        .lhs_var = lhs,
        .rhs_var = rhs,
    };
    ketl_hir_builder_insert_binary_op(pContext->p_state, &pContext->hir_builder, header, &instr);
    return output_var;
}

static uint16_t push_hir_argument(ketl_parser_context* pContext, ketl_hir_var_id_t var_id, uint16_t arguments_counter) {
    _ketl_parse_argument_stack_t_push_back_copy(&pContext->v_argument_stack, var_id);
    return ++arguments_counter;
}

static ketl_hir_var_id_t push_hir_call(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t callee_id, uint16_t arguments_count) {
    ketl_hir_header_t header = {
        .tag = KETL_HIR_CALL,
        .file_symbol = pContext->p_filename,
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
    };
    ketl_hir_var_id_t output_var = push_temp_var(pContext); 
    ketl_hir_call_t instr = {
        .output_var = output_var,
        .callee = callee_id,
        .arguments_count = arguments_count,
    };

    ketl_hir_builder_insert_call(pContext->p_state, &pContext->hir_builder, header, &instr,
        // pass pointer to last 'arguments_count' elements and immidiatly cut 'arguments_count' tail
        pContext->v_argument_stack.pData + (pContext->v_argument_stack.size -= arguments_count));
    return output_var;
}

static void push_hir_return_value(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t var_id) {
    ketl_hir_header_t header = {
        .tag = KETL_HIR_RETURN_VALUE,
        .file_symbol = pContext->p_filename,
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
    };
    ketl_hir_return_value_t instr = {
        .value_var = var_id,
    };
    ketl_hir_builder_insert_instr(&pContext->hir_builder, header, (uint8_t*)&instr);
}

static void push_hir_instr(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_hir_tag_t tag) {
    ketl_hir_header_t header = {
        .tag = tag,
        .file_symbol = pContext->p_filename,
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
    };
    ketl_hir_builder_insert_instr(&pContext->hir_builder, header,  NULL);
}

static ketl_hir_used_type_index_t find_type(ketl_parser_context* pContext, ketl_parse_node_output type_literal) {
    ketl_type* p_type = ketl_state_get_type(pContext->p_state, node_token_source(type_literal), type_literal.token.body.length);
    return ketl_hir_builder_get_used_type_index(&pContext->hir_builder, p_type);
}

static void push_hir_assign_impl(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t lhs_var, ketl_hir_var_id_t rhs_var) {

    ketl_hir_var_t* p_lhs_var = pContext->hir_builder.v_vars.pData + lhs_var;
    ketl_hir_var_t* p_rhs_var = pContext->hir_builder.v_vars.pData + rhs_var;

    if (p_lhs_var->uid == KETL_HIR_VAR_UID_LITERAL || p_lhs_var->info == KETL_HIR_VAR_INFO_TEMP) {
        // TODO error
        KETL_ASSERT(false);
    }
    
    // TODO casting if needed

    if (p_rhs_var->uid != KETL_HIR_VAR_UID_LITERAL&& p_rhs_var->info == KETL_HIR_VAR_INFO_TEMP) {
        ketl_hir_builder_replace_temp_var(&pContext->hir_builder, lhs_var, rhs_var);
        return;
    }

    ketl_hir_header_t assign_header = {
        .tag = KETL_HIR_ASSIGN,
        .file_symbol = pContext->p_filename,
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
    };
    ketl_hir_assign_t instr = {
        .dest_var = lhs_var,
        .source_var = rhs_var,
    };
    ketl_hir_builder_insert_instr(&pContext->hir_builder, assign_header, (uint8_t*)&instr);
}

static void push_hir_assign(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t lhs_var, ketl_hir_var_id_t rhs_var) {
    ketl_hir_var_t* p_lhs_var = pContext->hir_builder.v_vars.pData + lhs_var;
    if (p_lhs_var->uid == KETL_HIR_VAR_UID_LITERAL) {
        // TODO error
        KETL_ASSERT(false);
        return;
    }

    if (p_lhs_var->info != KETL_HIR_VAR_INFO_TEMP &&
        pContext->hir_builder.v_vars_infos.pData[p_lhs_var->info].p_global == NULL) {
        // TODO will not work in a looping scenario without f instruction at the begining of the block
        //lhs_var = ketl_hir_builder_increment_var_uid(&pContext->hir_builder, lhs_var);
        // TODO should not run during debug compilation
    }
    push_hir_assign_impl(pContext, p_pos_info, lhs_var, rhs_var);
}

static void push_hir_variable_declaration(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_parse_node_output id_literal, ketl_hir_used_type_index_t type_index, ketl_hir_var_id_t init_var) {
    ketl_hir_var_id_t id_var = ketl_hir_builder_register_var(&pContext->hir_builder, 
        push_symbol(pContext, id_literal), type_index);

    push_hir_assign_impl(pContext, p_pos_info, id_var, init_var);
}

// static ketl_hir_block_index_t reserve_hir_blocks(ketl_parser_context* pContext, uint8_t count) {
//     ketl_hir_block_index_t first_block = (ketl_hir_block_index_t)pContext->hir_builder.v_blocks.size;
//     _ketl_parse_flow_stack_t_reserve(&pContext->v_flow_stack, pContext->v_flow_stack.size + count);
//     hir_builder_blocks_t_reserve(&pContext->hir_builder.v_blocks, pContext->hir_builder.v_blocks.size + count);
//     for (uint8_t i = count; i > 0; --i) {
//         _ketl_parse_flow_stack_t_push_back_copy(&pContext->v_flow_stack, first_block + i - 1);
//         hir_builder_blocks_t_push_back_copy(&pContext->hir_builder.v_blocks, 0u);
//     }
//     return first_block;
// }

// static void pull_hir_flow_block(ketl_parser_context* pContext) {
//     KETL_ASSERT(pContext->v_flow_stack.size > 0);

//     uint32_t last_flow_index = --pContext->v_flow_stack.size;
//     ketl_hir_block_index_t last_flow_block = pContext->v_flow_stack.pData[last_flow_index];
//     pContext->hir_builder.v_blocks.pData[last_flow_block] = pContext->hir_builder.v_instrs.size;
// }

// static void push_hir_endif(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info) {
//     ketl_hir_header_t jump_header = {
//         .tag = KETL_HIR_JUMP,
//         .file_symbol = pContext->p_filename,
//         .start_line_index = p_pos_info->start_pos_line,
//         .end_line_index = p_pos_info->end_pos_line,
//         .start_col_index = p_pos_info->start_pos_col,
//         .end_col_index = p_pos_info->end_pos_col,
//     };
//     ketl_hir_jump_t instr = {
//         .block_index = pContext->next_block,
//     };
    
//     ketl_hir_builder_insert_instr(&pContext->hir_builder, jump_header, (uint8_t*)&instr);

//     pContext->next_block = (ketl_hir_block_index_t)-1;

//     pull_hir_flow_block(pContext);
// }

// static void push_hir_else(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info) {
//     ketl_hir_header_t jump_header = {
//         .tag = KETL_HIR_JUMP,
//         .file_symbol = pContext->p_filename,
//         .start_line_index = p_pos_info->start_pos_line,
//         .end_line_index = p_pos_info->end_pos_line,
//         .start_col_index = p_pos_info->start_pos_col,
//         .end_col_index = p_pos_info->end_pos_col,
//     };
//     ketl_hir_jump_t instr = {
//         .block_index = pContext->next_block,
//     };
    
//     ketl_hir_builder_insert_instr(&pContext->hir_builder, jump_header, (uint8_t*)&instr);

//     pull_hir_flow_block(pContext);
// }

// static void push_hir_if(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t bool_expr_var) {
//     // TODO check casting

//     ketl_hir_block_index_t first_block = reserve_hir_blocks(pContext, 3);
    
//     ketl_hir_header_t if_header = {
//         .tag = KETL_HIR_JUMP_IF,
//         .file_symbol = pContext->p_filename,
//         .start_line_index = p_pos_info->start_pos_line,
//         .end_line_index = p_pos_info->end_pos_line,
//         .start_col_index = p_pos_info->start_pos_col,
//         .end_col_index = p_pos_info->end_pos_col,
//     };
//     ketl_hir_jump_if_t instr = {
//         .true_block = first_block,
//         .false_block = first_block + 1,
//         .expr_var = bool_expr_var,
//     };
    
//     pContext->next_block = first_block + 2;
//     // TODO provide true and false branches
//     ketl_hir_builder_insert_instr(&pContext->hir_builder, if_header, (uint8_t*)&instr);

//     pull_hir_flow_block(pContext);
// }

#define call(function) ((function)(pContext))
#define call_n(function, ...) ((function)(pContext, __VA_ARGS__))
#define call_with_pos(function) ((function)(pContext, &pos_info))
#define call_n_with_pos(function, ...) ((function)(pContext, &pos_info, __VA_ARGS__))

#define EOF_STR "EOF"
#define get_token_length(token) ((int)((token).length > 0 ? (token).length : sizeof(EOF_STR) - 1))
#define get_token_source(pContext, token) ((token).length > 0 ? (pContext)->p_source + (pContext)->offset : EOF_STR)

char error_buffer[256];

#define error(...) \
do {\
    int __count = snprintf(error_buffer, KETL_ARRAY_SIZE(error_buffer), __VA_ARGS__);\
    string_builder_t_push_back_ref_n(&pContext->p_state->error_stream, error_buffer, __count);\
    string_builder_t_push_back_copy(&pContext->p_state->error_stream, '\n');\
} while (0)
#define error_expected_expr() error("expected expr, got '%.*s'", get_token_length(token), get_token_source(pContext, token))
#define error_expected_terminator() error("expected ';', got '%.*s'", get_token_length(token), get_token_source(pContext, token))

#ifndef NDEBUG
//#define DRAW_STACK_INFO
#endif

#define DRAW_STACK(pContext)\
do {\
    for (uint32_t i = 0; i < (pContext)->v_node_stack.size; ++i) {\
        printf("%"PRIu16"('%.*s') ", (pContext)->v_node_stack.pData[i].state,\
            (pContext)->v_node_stack.pData[i].pos_info.end_pos_col - (pContext)->v_node_stack.pData[i].pos_info.start_pos_col,\
            (pContext)->p_source + (pContext)->v_node_stack.pData[i].pos_info.start_pos_col);\
    }\
    printf("\n");\
    /*printf("stack size %d\n", (pContext)->v_node_stack).size;*/\
} while(false)

#include "compiler/parser_lalr.cxx"

void ketl_parser_build_hir(ketl_state* p_state, ketl_hir_t* p_hir, const char* p_filename, const char* p_source, uint32_t length, const ketl_allocator* p_allocator) {
    *p_hir = (ketl_hir_t){0};

    ketl_parser_context context = {.p_state = p_state, .p_source = p_source, .offset = 0};
    ketl_hir_builder_init(&context.hir_builder, p_allocator);
    context.p_filename = (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&context.hir_builder.symbols, p_filename, KETL_NULL_TERMINATED_LENGTH_32);
    context.next_block = (ketl_hir_block_index_t)-1;

    uint32_t count = 0;
    ketl_token* pTokens = ketl_lexer_build_tokens(p_source, length, &count, p_allocator);
    
    if (pTokens != NULL) {
        _ketl_parse_node_stack_t_init(&context.v_node_stack, 4, p_allocator);
        _ketl_parse_node_stack_t_push_back_copy(&context.v_node_stack, (ketl_parse_node){.state=0, .output={0}});
        _ketl_parse_argument_stack_t_init(&context.v_argument_stack, 4, p_allocator);
        _ketl_parse_flow_stack_t_init(&context.v_flow_stack, 4, p_allocator);

        uint32_t i = 0u;
        for (; i < count; ++i) {
            const ketl_token token = pTokens[i];
            context.offset += token.prevOffset;
    #ifdef DRAW_STACK_INFO
            printf("%d with value %.*s at %d\n", (int)token.type, token.length, p_source + context.offset, context.offset);
    #endif

            if (ketl_parser_process_token(&context, token)) {
                break;
            }

            context.offset += token.length;
        }

        if (i >= count) {
            ketl_parser_process_token(&context, (ketl_token){
                .type = KETL_TOKEN_TYPE_TOTAL,
                .length = 0,
                .prevOffset = 0
            });
        }

    #ifdef DRAW_STACK_INFO
        printf("total stack: %d\n", context.v_node_stack.capacity);
        printf("last stack: %d\n", context.v_node_stack.size);
        printf("result: %d\n", context.v_node_stack.pData[context.v_node_stack.size - (1)].output.var_id);
        DRAW_STACK(&context);
        for (uint32_t i = 0u; i < context.hir_builder.symbols.vStorage.size; ++i) {
            if (i != 0 && (i & 15) == 0) {
                printf("\n");
            }
            char character = context.hir_builder.symbols.vStorage.pData[i];
            printf("%c", character == '\0' ? '*' : character);
        }
        printf("\n");
    #endif
        if (context.v_node_stack.size != 2 || context.v_node_stack.pData[1].state != 1) {
            // TODO error?
            // possibly not possible after proper error restoring
            return;
        }

        _ketl_parse_flow_stack_t_deinit(&context.v_flow_stack);
        _ketl_parse_argument_stack_t_deinit(&context.v_argument_stack);
        _ketl_parse_node_stack_t_deinit(&context.v_node_stack);

        ketl_free(p_allocator, pTokens);
    }

    ketl_hir_builder_flush(p_state, &context.hir_builder, p_hir);

    return;
}
