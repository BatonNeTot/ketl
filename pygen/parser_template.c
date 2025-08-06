//🫖ketl
#include "compiler/parser.h"

#include "ketl_impl.h"

#include "compiler/lexer.h"

#include "containers/vector.h"
#include "containers/hash_map.h"
#include "memory_impl.h"
#include "str.h"

#include <stdio.h>

typedef uint16_t action;
#define ACTION_MASK_TYPE   0x0003
#define ACTION_SHIFT_TYPE  0
#define ACTION_MASK_VALUE  0xFFFC
#define ACTION_SHIFT_VALUE 2

#define ACTION_TYPE_SHIFT  0
#define ACTION_TYPE_REDUCE 1
#define ACTION_TYPE_ERROR  2
#define ACTION_TYPE_ACCEPT 3

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

KETL_DEFINE(ketl_parse_node) {
    uint32_t state;
    ketl_parse_pos_info pos_info;
    ketl_parse_node_output output;
};

KETL_VECTOR_DECLARATION(_ketl_parse_node_stack_t, ketl_parse_node)
KETL_VECTOR_DEFINITION(_ketl_parse_node_stack_t, ketl_parse_node)

KETL_VECTOR_DECLARATION(_ketl_parse_argument_stack_t, ketl_hir_var_id_t)
KETL_VECTOR_DEFINITION(_ketl_parse_argument_stack_t, ketl_hir_var_id_t)

KETL_DEFINE(ketl_parser_context) {
    ketl_state* p_state;
    const char* p_source;
    uint32_t offset;
    ketl_hir_symbol_offset_t p_filename;
    ketl_hir_builder_t hir_builder;
    _ketl_parse_node_stack_t v_node_stack;
    _ketl_parse_argument_stack_t v_argument_stack;
};

static uint16_t actionsTable[] = {
    $actionTableBody
};

static uint16_t gotoTable[] = {
    $gotoTableBody
};

static uint16_t prodNontermsArray[] = {
    $prodNontermsArrayBody
};

static uint8_t prodLengthsArray[] = {
    $prodLengthsArrayBody
};

#define stack_top_node(index) (pContext->v_node_stack.pData[pContext->v_node_stack.size - (index)])
#define stack_top(index) (stack_top_node(index).output)
#define node_token_source(ketl_parse_node_output) (pContext->p_source + (ketl_parse_node_output).token.offset)

#define TERMS_COUNT (KETL_TOKEN_TYPE_TOTAL + 2) // additional last term and error term
#define NONTERMS_COUNT $nontermCount

static uint16_t push_symbol(ketl_parser_context* pContext, ketl_parse_node_output literal) {
    return ketl_atomic_strings_get(&pContext->hir_builder.symbols, node_token_source(literal), literal.token.body.length);
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

static ketl_hir_var_id_t push_hir_binary_op(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_hir_tag_t hir_tag, ketl_hir_var_id_t lhs, ketl_hir_var_id_t rhs) {
    ketl_hir_header_t header = {
        .tag = hir_tag,
        .file_symbol = pContext->p_filename,
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
    };
    ketl_hir_var_id_t output_var = ketl_hir_builder_create_temp_var(&pContext->hir_builder, KETL_HIR_USED_TYPE_UNKHOWN); 
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
    ketl_hir_var_id_t output_var = ketl_hir_builder_create_temp_var(&pContext->hir_builder, KETL_HIR_USED_TYPE_UNKHOWN); 
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

static void push_hir_assign(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_hir_var_id_t lhs_var, ketl_hir_var_id_t rhs_var) {
    // TODO casting if needed
    // TODO check that lhs_var is not literal or temp
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

static void push_hir_variable_declaration(ketl_parser_context* pContext, ketl_parse_pos_info* p_pos_info, ketl_parse_node_output id_literal, ketl_hir_used_type_index_t type_index, ketl_hir_var_id_t init_var) {
    // TODO casting if needed
    ketl_hir_var_id_t id_var = ketl_hir_builder_register_var(&pContext->hir_builder, 
        push_symbol(pContext, id_literal), type_index);
    ketl_hir_header_t assign_header = {
        .tag = KETL_HIR_ASSIGN,
        .file_symbol = pContext->p_filename,
        .start_line_index = p_pos_info->start_pos_line,
        .end_line_index = p_pos_info->end_pos_line,
        .start_col_index = p_pos_info->start_pos_col,
        .end_col_index = p_pos_info->end_pos_col,
    };
    ketl_hir_assign_t instr = {
        .dest_var = id_var,
        .source_var = init_var,
    };
    ketl_hir_builder_insert_instr(&pContext->hir_builder, assign_header, (uint8_t*)&instr);
}

#define call(function, ...) ((function)(pContext, __VA_ARGS__))
#define call_with_pos(function, ...) ((function)(pContext, &pos_info, __VA_ARGS__))

#ifndef NDEBUG
//#define DRAW_STACK_INFO
#endif

#define DRAW_STACK(stack)\
do {\
    for (uint32_t i = 0; i < (stack).size; ++i) {\
        printf("%d(%d) ", (stack).pData[i].state, (stack).pData[i].output.token.body.type);\
    }\
    printf("\n");\
    /*printf("stack size %d\n", (stack).size);*/\
} while(false)

static bool ketl_parser_process_token(ketl_parser_context* pContext, const ketl_token token) {
    KETL_FOREVER {
        uint64_t state = stack_top_node(1).state;
        uint16_t action = actionsTable[state * TERMS_COUNT + token.type];
#ifdef DRAW_STACK_INFOstart_pos_line
        printf("parsing token %d\n", token.type);
		printf("from state %lld and term %d\n", state, token.type);
        printf("actionCode %d, ", action);
        printf("action %d\n", (action & ACTION_MASK_TYPE) >> ACTION_SHIFT_TYPE);
#endif

        switch ((action & ACTION_MASK_TYPE) >> ACTION_SHIFT_TYPE) {
            case ACTION_TYPE_SHIFT: {
                _ketl_parse_node_stack_t_push_back_copy(&pContext->v_node_stack, (ketl_parse_node){.state = (action & ACTION_MASK_VALUE) >> ACTION_SHIFT_VALUE, 
                    .output = {
                        .token = {
                            .body = token,
                            .offset = pContext->offset,
                        },
                    },
                    });
#ifdef DRAW_STACK_INFO
                printf("pushing %d\n", (action & ACTION_MASK_VALUE) >> ACTION_SHIFT_VALUE);
                DRAW_STACK(pContext->v_node_stack);
#endif
                return false;
            }
            case ACTION_TYPE_REDUCE: {
                uint16_t prodIndex = (action & ACTION_MASK_VALUE) >> ACTION_SHIFT_VALUE;
                uint16_t prodNonterm = prodNontermsArray[prodIndex];
                ketl_parse_node_output result = {0};
                ketl_parse_node* first_node = &stack_top_node(prodLengthsArray[prodIndex]);
                ketl_parse_node* last_node = &stack_top_node(1);
                ketl_parse_pos_info pos_info = {
                    .start_pos_line = first_node->pos_info.start_pos_line,
                    .end_pos_line = last_node->pos_info.end_pos_line,
                    .start_pos_col = first_node->pos_info.start_pos_col,
                    .end_pos_col = last_node->pos_info.end_pos_col,
                };
#ifdef DRAW_STACK_INFO
                printf("reducing %d\n", prodLengthsArray[prodIndex]);
#endif
                switch (prodIndex) {
                    $productionActionsSwitch
                }
                _ketl_parse_node_stack_t_resize(&pContext->v_node_stack, pContext->v_node_stack.size - prodLengthsArray[prodIndex]);
                uint64_t stateAfterReduction = stack_top_node(1).state;
                uint64_t gotoState = gotoTable[stateAfterReduction * NONTERMS_COUNT + prodNonterm];
#ifdef DRAW_STACK_INFO
                DRAW_STACK(pContext->v_node_stack);
				printf("from state %lld and nonterm %d\n", stateAfterReduction, prodNonterm);
                printf("pushing %lld\n", gotoState);
#endif
                _ketl_parse_node_stack_t_push_back_copy(&pContext->v_node_stack, (ketl_parse_node){
                    .state=gotoState, 
                    .output=result,
                    .pos_info=pos_info,
                    });
#ifdef DRAW_STACK_INFO
                DRAW_STACK(pContext->v_node_stack);
#endif
                if (gotoState == (uint64_t)-1) {
                    printf("uknown goto by nonterm {productionInfo.nonterm} of length {productionInfo.length}!\n");
                }
                continue;
            }
            case ACTION_TYPE_ERROR: {
                printf("can't process %d!\n", token.type);
                DRAW_STACK(pContext->v_node_stack);
                return true;
            }
            case ACTION_TYPE_ACCEPT: {
                //printf("accept!\n");
                return true;
            }
        }
        
        return (uint64_t)-1;
    }
}

void ketl_parser_build_hir(ketl_state* p_state, ketl_hir_t* p_hir, const char* p_filename, const char* p_source, uint32_t length, const ketl_allocator* p_allocator) {
    *p_hir = (ketl_hir_t){0};

    uint32_t count = 0;
    ketl_token* pTokens = ketl_lexer_build_tokens(p_source, length, &count, p_allocator);
    
    if (pTokens == NULL) {
        return;
    }

    ketl_parser_context context = {.p_state = p_state, .p_source = p_source, .offset = 0};
    ketl_hir_builder_init(&context.hir_builder, p_allocator);
    context.p_filename = ketl_atomic_strings_get(&context.hir_builder.symbols, p_filename, KETL_NULL_TERMINATED_LENGTH_32);
    _ketl_parse_node_stack_t_init(&context.v_node_stack, 4, p_allocator);
    _ketl_parse_node_stack_t_push_back_copy(&context.v_node_stack, (ketl_parse_node){.state=0, .output={0}});
    _ketl_parse_argument_stack_t_init(&context.v_argument_stack, 4, p_allocator);

    for (uint32_t i = 0u; i < count; ++i) {
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

    ketl_parser_process_token(&context, (ketl_token){
        .type = KETL_TOKEN_TYPE_TOTAL,
        .length = 0,
        .prevOffset = 0
        });

#ifdef DRAW_STACK_INFO
    printf("total stack: %d\n", context.v_node_stack.capacity);
    printf("last stack: %d\n", context.v_node_stack.size);
    printf("result: %d\n", context.v_node_stack.pData[context.v_node_stack.size - (1)].result);
    DRAW_STACK(context.v_node_stack);
    for (uint32_t i = 0u; i < context.vSymbols.size; ++i) {
        if (i != 0 && (i & 15) == 0) {
            printf("\n");
        }
        char character = context.vSymbols.pData[i];
        printf("%c", character == '\0' ? '*' : character);
    }
    printf("\n");
#endif
    if (context.v_node_stack.size != 2 || context.v_node_stack.pData[1].state != 1) {
        // Error!
        // TODO mark for error
        KETL_ASSERT(false);
    }

    _ketl_parse_argument_stack_t_deinit(&context.v_argument_stack);
    _ketl_parse_node_stack_t_deinit(&context.v_node_stack);

    ketl_free(p_allocator, pTokens);

    ketl_hir_builder_flush(&context.hir_builder, p_hir);
}
