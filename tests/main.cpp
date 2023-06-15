/*🍲Ketl🍲*/
#include <iostream>

extern "C" {
#include "compiler/token.h"
#include "compiler/lexer.h"
#include "compiler/bnf_solver.h"
#include "compiler/bnf_node.h"
#include "compiler/bnf_scheme.h"
#include "compiler/syntax_node.h"
#include "ketl/object_pool.h"
}

int main(int argc, char** argv) {

	auto source = "let test = 0;";

	KETLObjectPool syntaxNodePool;
	ketlInitObjectPool(&syntaxNodePool, sizeof(KETLSyntaxNode), 16);

	KETLObjectPool bnfNodePool;
	ketlInitObjectPool(&bnfNodePool, sizeof(KETLBnfNode), 16);

	KETLBnfNode* bnfScheme = ketlBuildBnfScheme(&bnfNodePool);

	KETLLexer lexer;
	ketlInitLexer(&lexer, source, KETL_LEXER_SOURCE_NULL_TERMINATED);

	while (ketlHasNextToken(&lexer)) {
		auto token = ketlGetNextToken(&lexer);
		std::cout << '"' << std::string_view(token->value, token->length) << '"' << std::endl;
		ketlFreeToken(token);
	}

	auto* root = ketlSolveBnf(bnfScheme, &syntaxNodePool);

	ketlDeinitObjectPool(&bnfNodePool);
	ketlDeinitObjectPool(&syntaxNodePool);

	return 0;
}