/*🍲Ketl🍲*/
#include <iostream>

extern "C" {
#include "compiler/token.h"
#include "compiler/lexer.h"
#include "compiler/syntax_solver.h"
#include "compiler/bnf_node.h"
#include "compiler/bnf_scheme.h"
#include "compiler/syntax_node.h"
#include "ketl/object_pool.h"
}

#include <crtdbg.h>

int main(int argc, char** argv) {
	
	auto source = "let test1 := 5 + 10; let test2 := test1 - 17;";

	KETLObjectPool tokenPool;
	ketlInitObjectPool(&tokenPool, sizeof(KETLToken), 16);

	KETLObjectPool syntaxNodePool;
	ketlInitObjectPool(&syntaxNodePool, sizeof(KETLSyntaxNode), 16);

	KETLObjectPool bnfNodePool;
	ketlInitObjectPool(&bnfNodePool, sizeof(KETLBnfNode), 16);

	KETLBnfNode* bnfScheme = ketlBuildBnfScheme(&bnfNodePool);

	KETLLexer lexer;
	ketlInitLexer(&lexer, source, KETL_LEXER_SOURCE_NULL_TERMINATED, &tokenPool);

	if (!ketlHasNextToken(&lexer)) {
		return -1;
	}
	KETLToken* firstToken = ketlGetNextToken(&lexer);
	KETLToken* token = firstToken;

	while (ketlHasNextToken(&lexer)) {
		token = token->next = ketlGetNextToken(&lexer);
	}
	token->next = nullptr;

	auto* root = ketlSolveBnf(firstToken, bnfScheme, &syntaxNodePool);

	ketlDeinitObjectPool(&bnfNodePool);
	ketlDeinitObjectPool(&syntaxNodePool);
	ketlDeinitObjectPool(&tokenPool);

	return 0;
}