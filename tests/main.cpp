/*🍲Ketl🍲*/
#include <iostream>

extern "C" {
#include "compiler/lexer.h"
#include "compiler/token.h"
}

int main(int argc, char** argv) {

	auto source = " +.0 ";

	auto lexer = ketlCreateLexer(source, KETL_LEXER_SOURCE_NULL_TERMINATED);

	while (ketlHasNextToken(lexer)) {
		auto token = ketlGetNextToken(lexer);
		std::cout << '"' << std::string_view(token->value, token->length) << '"' << std::endl;
	}

	ketlFreeLexer(lexer);

	return 0;
}