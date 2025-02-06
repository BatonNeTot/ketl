//🫖ketl
#include "ketl_impl.h"

#include "compiler/parser.h"
#include "compiler/bytecoder.h"
#include "compiler/assembler.h"

#include "executable_memory.h"
#include "execution.h"
#include "memory_impl.h"

#include <stdio.h>

ketl_state* ketl_state_create(const ketl_allocator* pAllocator) {
    ketl_state* pState = ketl_alloc(pAllocator, sizeof(ketl_state));
    *pState = (ketl_state){
        .pAllocator = pAllocator
    };

    ketl_gc_init(&pState->gc, pAllocator);

    return pState;
}

void ketl_state_destroy(ketl_state* pState) {
    ketl_gc_deinit(&pState->gc);

    ketl_free(pState->pAllocator, pState);
}

void ketl_state_eval(ketl_state* pState, const char* pSource, uint32_t length) {
    ketl_state_eval_int64(pState, pSource, length);
}

int64_t ketl_state_eval_int64(ketl_state* pState, const char* pSource, uint32_t length) {
    ketl_ir ir = ketl_parser_parser(pSource, length, pState->pAllocator);

    for (uint32_t i = 0u; i < ir.nodesCount; ++i) {
        char buffer[256];
        uint32_t length = ketl_ir_node_format(ir.pNodes[i], ir.pSymbols, buffer, sizeof(buffer) / sizeof(*buffer));
        printf("(%d) %.*s\n", i, length, buffer);
    }

    ketl_bytecode bytecode = ketl_bytecode_compile(ir, pState->pAllocator);
    ketl_free(pState->pAllocator, ir.pNodes);
    ketl_free(pState->pAllocator, ir.pSymbols);

    for (uint32_t i = 0u; i < bytecode.instructionsCount; 
            i += ketl_bytecode_decode_instruction_length(bytecode.pInstructions[i])) {
        char buffer[256];
        uint32_t length = ketl_bytecode_format(bytecode.pInstructions + i, bytecode.pLabels, buffer, sizeof(buffer) / sizeof(*buffer));
        printf("%d: %.*s\n", i, length, buffer);
    }

    uint32_t opcodesSize = 0u;
    uint8_t* pOpcodes = ketl_assembler_compile(bytecode, &opcodesSize, pState->pAllocator);
    ketl_free(pState->pAllocator, bytecode.pInstructions);

    {
        char buffer[1024];
        uint32_t length = ketl_assembler_format(pOpcodes, opcodesSize, buffer, sizeof(buffer) / sizeof(*buffer));
        printf("%.*s\n", length, buffer);
    }

    ketl_executable_memory ex_memory;
    ketl_executable_memory_init(&ex_memory, pState->pAllocator);

    uint8_t* executableOpcodes = ketl_executable_memory_allocate(&ex_memory, pOpcodes, opcodesSize);
    ketl_free(pState->pAllocator, pOpcodes);
    int64_t result = ketl_execute(&executableOpcodes);

    ketl_executable_memory_deinit(&ex_memory);

    return result;
}