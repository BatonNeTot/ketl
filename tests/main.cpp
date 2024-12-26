//🫖ketl
#include "ketl/ketl.hpp"

extern "C" {
    #include "compiler/parser.h"
}

#include <iostream>

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

    const char* source = "2 + 3 * 5;";
    auto ir = ketl_parser_parser(source, KETL_NULL_TERMINATED_LENGTH_32);

    for (auto i = 0u; i < ir.nodesCount; ++i) {
        char buffer[256];
        auto length = ketl_ir_node_format(ir.pNodes[i], ir.pSymbols, buffer, sizeof(buffer) / sizeof(*buffer));
        printf("(%d) %.*s\n", i, length, buffer);
    }
}
