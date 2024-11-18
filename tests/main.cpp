//🫖ketl
#include "ketl/ketl.hpp"

extern "C" {
    #include "compiler/lexer.h"
}

#include <iostream>

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

    uint32_t count = 0;
    const char* source = "int test = 0;";
    const ketl_token* tokens = ketl_lexer_build_tokens(source, KETL_NULL_TERMINATED_LENGTH_32, &count);
    auto additionalString = "\n";
    (void)additionalString;
    
    if (tokens) {
        uint32_t pos = 0;
        for (auto i = 0u; i < count; ++i) {
            pos += tokens[i].prevOffset;
            std::cout << (int)tokens[i].type << " with value " << std::string_view{source + pos, tokens[i].length} << " at " << pos << std::endl;//"\n";
            pos += tokens[i].length;
        }
        std::cout.flush();
    }
}
