//🫖ketl
#include "ketl/ketl.hpp"

extern "C" {
    #include "compiler/parser.h"
    #include "compiler/bytecoder.h"
    #include "compiler/assembler.h"
    #include "executable_memory.h"
    #include "execution.h"
}

#include <iostream>

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

    const char* source = "return 2 + 5 * 8;";
    auto ir = ketl_parser_parser(source, KETL_NULL_TERMINATED_LENGTH_32);

    for (auto i = 0u; i < ir.nodesCount; ++i) {
        char buffer[256];
        auto length = ketl_ir_node_format(ir.pNodes[i], ir.pSymbols, buffer, sizeof(buffer) / sizeof(*buffer));
        printf("(%d) %.*s\n", i, length, buffer);
    }

    auto bytecode = ketl_bytecode_compile(ir);

    for (auto i = 0u; i < bytecode.instructionsCount; 
            i += ketl_bytecode_decode_instruction_length(bytecode.pInstructions[i])) {
        char buffer[256];
        auto length = ketl_bytecode_format(bytecode.pInstructions + i, bytecode.pLabels, buffer, sizeof(buffer) / sizeof(*buffer));
        printf("%d: %.*s\n", i, length, buffer);
    }

    uint32_t opcodesSize = 0u;
    auto pOpcodes = ketl_assembler_compile(bytecode, &opcodesSize);

    {
        char buffer[1024];
        auto length = ketl_assembler_format(pOpcodes, opcodesSize, buffer, sizeof(buffer) / sizeof(*buffer));
        printf("%.*s\n", length, buffer);
    }

    ketl_executable_memory ex_memory;
    ketl_executable_memory_init(&ex_memory);

    auto executableOpcodes = ketl_executable_memory_allocate(&ex_memory, pOpcodes, opcodesSize);
    int result = ketl_execute(&executableOpcodes);
    printf("result = %d\n", result);

    ketl_executable_memory_deinit(&ex_memory);
}
