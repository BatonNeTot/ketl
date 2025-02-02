//🫖ketl
#include "compiler/parser.h"

#include "compiler/lexer.h"

#include "containers/vector.h"
#include "containers/hash_map.h"
#include "memory_impl.h"
#include "str.h"

#include <stdio.h>

#include <assert.h>

typedef uint16_t action;
#define ACTION_MASK_TYPE   0x0003
#define ACTION_SHIFT_TYPE  0
#define ACTION_MASK_VALUE  0xFFFC
#define ACTION_SHIFT_VALUE 2

#define ACTION_TYPE_SHIFT  0
#define ACTION_TYPE_REDUCE 1
#define ACTION_TYPE_ERROR  2
#define ACTION_TYPE_ACCEPT 3

KETL_DEFINE(ketl_parse_node) {
    uint32_t state;
    union {
        uint16_t result;
        struct {
            ketl_token token;
            uint32_t offset;
        };
    };
};

KETL_VECTOR_DECLARATION(ketl_parse_node)
KETL_VECTOR_DEFINITION(ketl_parse_node)

KETL_VECTOR_DECLARATION(ketl_ir_node)
KETL_VECTOR_DEFINITION(ketl_ir_node)

KETL_NAMED_VECTOR_DECLARATION(symbols, char)
KETL_NAMED_VECTOR_DEFINITION(symbols, char)

KETL_NAMED_HASH_MAP_DECLARATION(symbols_map, const char*, uint16_t)
KETL_NAMED_HASH_MAP_DEFINITION(symbols_map, const char*, uint16_t, ketl_str_hash, ketl_str_is_equal)

KETL_DEFINE(ketl_parser_context) {
    const char* pSource;
    uint32_t offset;
    uint16_t tempVarIndex;
    ketl_parse_node_vector vStack;
    ketl_ir_node_vector vNodes;
    symbols vSymbols;
    symbols_map mSymbolsMap;
};

uint16_t actionsTable[] = {
    $actionTableBody
};

uint16_t gotoTable[] = {
    $gotoTableBody
};

uint16_t prodNontermsArray[] = {
    $prodNontermsArrayBody
};

uint8_t prodLengthsArray[] = {
    $prodLengthsArrayBody
};

#define STACK_TOP(index) (pContext->vStack.pData[pContext->vStack.size - (index)])
#define NODE_TOKEN_SOURCE(ketl_parse_node) (pContext->pSource + (ketl_parse_node).offset)
#define PARSE_UINT_NODE(ketl_parse_node) strtoul(NODE_TOKEN_SOURCE(ketl_parse_node), NULL, 10)

#define TERMS_COUNT (KETL_TOKEN_TYPE_TOTAL + 2) // additional last term and error term
#define NONTERMS_COUNT $nontermCount

static uint16_t push_symbol(ketl_parser_context* pContext, const char* pSymbol, uint16_t length) {
    symbols_map* pmSymbolsMap = &pContext->mSymbolsMap;
    symbols_map_bucket* pSymbolBucket = symbols_map_get_or_insert_copy(pmSymbolsMap, pSymbol, 0);
    if (pSymbolBucket->key == pSymbol) {
        char* pCheckData = pContext->vSymbols.pData;
        const char* pAtomicSymbol = symbols_push_back_ref_n(&pContext->vSymbols, pSymbol, length);
        if (pCheckData != pContext->vSymbols.pData) {
            pCheckData = pContext->vSymbols.pData;
            KETL_NAMED_HASH_MAP_FOREACH(symbols_map, const char*, uint16_t, pmSymbolsMap, 
            __pBucket->key = pCheckData + __pBucket->value;);
        }
        pSymbolBucket->key = pAtomicSymbol;
        pSymbolBucket->value = pAtomicSymbol - pContext->vSymbols.pData;
    }
    return pSymbolBucket->value;
}

static uint16_t push_top_literal(ketl_parser_context* pContext) {
    char pBuffer[64] = {'#'};
    ketl_parse_node topNode = STACK_TOP(1);
    ketl_memcpy(pBuffer + 1, NODE_TOKEN_SOURCE(topNode), topNode.token.length);
    *(pBuffer + 1 + topNode.token.length) = '\0'; 
    return push_symbol(pContext, pBuffer, topNode.token.length + 2);
}

static uint16_t push_node_and_return(ketl_parser_context* pContext, ketl_ir_type type, uint16_t arg0, uint16_t arg1, uint16_t arg2, uint16_t result) {
    ketl_ir_node_vector_push_back_copy(&pContext->vNodes, (ketl_ir_node){
        .type = type,
        .args = { arg0, arg1, arg2 },
    });
    return result;
}

static uint16_t push_node_with_temp_var(ketl_parser_context* pContext, ketl_ir_type type, uint16_t arg1, uint16_t arg2) {
    char pBuffer[64] = {'~'};
    uint16_t length = snprintf(pBuffer + 1, sizeof(pBuffer) / sizeof(*pBuffer) - 1, "%d", pContext->tempVarIndex++);
    uint16_t arg0 = push_symbol(pContext, pBuffer, length + 2);
    ketl_ir_node_vector_push_back_copy(&pContext->vNodes, (ketl_ir_node){
        .type = type,
        .args = { arg0, arg1, arg2 },
    });
    return arg0;
}

#define PUSH_TOP_LITERAL()  (push_top_literal(pContext))
#define PUSH_NODE_AND_RETURN(type, arg0, arg1, arg2, result) (push_node_and_return(pContext, (type), (arg0), (arg1), (arg2), (result)))
#define PUSH_NODE_WTIH_TEMP_VAR(type, arg1, arg2) (push_node_with_temp_var(pContext, (type), (arg1), (arg2)))

#ifndef NDEBUG
//#define DRAW_STACK_INFO
#endif

#define DRAW_STACK(stack)\
do {\
    for (uint32_t i = 0; i < (stack).size; ++i) {\
        printf("%d(%d) ", (stack).pData[i].state, (stack).pData[i].result);\
    }\
    printf("\n");\
    /*printf("stack size %d\n", (stack).size);*/\
} while(false)

static bool ketl_parser_process_token(ketl_parser_context* pContext, const ketl_token token) {
    KETL_FOREVER {
        uint64_t state = STACK_TOP(1).state;
        uint16_t action = actionsTable[state * TERMS_COUNT + token.type];
#ifdef DRAW_STACK_INFO
        printf("parsing token %d\n", token.type);
		printf("from state %lld and term %d\n", state, token.type);
        printf("actionCode %d, ", action);
        printf("action %d\n", (action & ACTION_MASK_TYPE) >> ACTION_SHIFT_TYPE);
#endif

        switch ((action & ACTION_MASK_TYPE) >> ACTION_SHIFT_TYPE) {
            case ACTION_TYPE_SHIFT: {
                ketl_parse_node_vector_push_back_copy(&pContext->vStack, (ketl_parse_node){.state = (action & ACTION_MASK_VALUE) >> ACTION_SHIFT_VALUE, 
                    .token = token,
                    .offset = pContext->offset,
                    });
#ifdef DRAW_STACK_INFO
                printf("pushing %d\n", (action & ACTION_MASK_VALUE) >> ACTION_SHIFT_VALUE);
                DRAW_STACK(pContext->vStack);
#endif
                return false;
            }
            case ACTION_TYPE_REDUCE: {
                uint16_t prodIndex = (action & ACTION_MASK_VALUE) >> ACTION_SHIFT_VALUE;
                uint16_t prodNonterm = prodNontermsArray[prodIndex];
                uint64_t result = 0;
#ifdef DRAW_STACK_INFO
                printf("reducing %d\n", prodLengthsArray[prodIndex]);
#endif
                switch (prodIndex) {
                    $productionActionsSwitch
                }
                ketl_parse_node_vector_resize(&pContext->vStack, pContext->vStack.size - prodLengthsArray[prodIndex]);
                uint64_t stateAfterReduction = STACK_TOP(1).state;
                uint64_t gotoState = gotoTable[stateAfterReduction * NONTERMS_COUNT + prodNonterm];
#ifdef DRAW_STACK_INFO
                DRAW_STACK(pContext->vStack);
				printf("from state %lld and nonterm %d\n", stateAfterReduction, prodNonterm);
                printf("pushing %lld\n", gotoState);
#endif
                ketl_parse_node_vector_push_back_copy(&pContext->vStack, (ketl_parse_node){.state=gotoState, .result=result});
#ifdef DRAW_STACK_INFO
                DRAW_STACK(pContext->vStack);
#endif
                if (gotoState == (uint64_t)-1) {
                    printf("uknown goto by nonterm {productionInfo.nonterm} of length {productionInfo.length}!\n");
                }
                continue;
            }
            case ACTION_TYPE_ERROR: {
                printf("can't process %d!\n", token.type);
                DRAW_STACK(pContext->vStack);
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

ketl_ir ketl_parser_parser(const char* pSource, uint32_t length, const ketl_allocator* pAllocator) {
    ketl_ir ir = {.pNodes = NULL, .pSymbols = NULL, .nodesCount = 0};

    uint32_t count = 0;
    ketl_token* pTokens = ketl_lexer_build_tokens(pSource, length, &count, pAllocator);
    
    if (pTokens) {
        ketl_parser_context context = {.pSource = pSource, .offset = 0, .tempVarIndex = 0};
        ketl_parse_node_vector_init(&context.vStack, 4, pAllocator);
        ketl_parse_node_vector_push_back_copy(&context.vStack, (ketl_parse_node){.state=0, .result=0});
        ketl_ir_node_vector_init(&context.vNodes, count, pAllocator);
        symbols_init(&context.vSymbols, 4, pAllocator);
        symbols_map_init(&context.mSymbolsMap, pAllocator);

        for (uint32_t i = 0u; i < count; ++i) {
            const ketl_token token = pTokens[i];
            context.offset += token.prevOffset;
#ifdef DRAW_STACK_INFO
            printf("%d with value %.*s at %d\n", (int)token.type, token.length, pSource + context.offset, context.offset);
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
        printf("total stack: %d\n", context.vStack.capacity);
        printf("last stack: %d\n", context.vStack.size);
        printf("result: %d\n", context.vStack.pData[context.vStack.size - (1)].result);
        DRAW_STACK(context.vStack);
        for (uint32_t i = 0u; i < context.vSymbols.size; ++i) {
            if (i != 0 && (i & 15) == 0) {
                printf("\n");
            }
            char character = context.vSymbols.pData[i];
            printf("%c", character == '\0' ? '*' : character);
        }
        printf("\n");
#endif
        if (context.vStack.size != 2 || context.vStack.pData[1].state != 1) {
            // Error!
            // TODO mark for error
            assert(false);
        }

        ketl_parse_node_vector_deinit(&context.vStack);
        symbols_map_deinit(&context.mSymbolsMap);

        ketl_free(pAllocator, pTokens);

        ir.pNodes = context.vNodes.pData;
        ir.nodesCount = context.vNodes.size;
        ir.pSymbols = context.vSymbols.pData;
        return ir;
    }

    return ir;
}
