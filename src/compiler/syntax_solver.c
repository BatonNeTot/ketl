//🍲ketl
#include "ketl/compiler/syntax_solver.h"

#include "bnf_parser.h"
#include "syntax_parser.h"

#include "token.h"
#include "lexer.h"
#include "syntax_node.h"
#include "bnf_node.h"
#include "bnf_scheme.h"
#include "ketl/object_pool.h"
#include "ketl/stack.h"

#include <stdio.h>

#define PRINT_SPACE(x) printf("%*s", (x), " ")

static void printBnfSolution(KETLStackIterator* iterator) {
	KETLStack parentStack;
	ketlInitStack(&parentStack, sizeof(void*), 16);
	int deltaOffset = 4;
	int currentOffset = 0;
	while (ketlIteratorStackHasNext(iterator)) {
		KETLBnfParserState* solverState = ketlIteratorStackGetNext(iterator);

		KETLBnfParserState* peeked;
		while (!ketlIsStackEmpty(&parentStack) && solverState->parent != (peeked = *(KETLBnfParserState**)ketlPeekStack(&parentStack))) {
			ketlPopStack(&parentStack);
			switch (peeked->bnfNode->type) {
			case KETL_BNF_NODE_TYPE_REF:
			case KETL_BNF_NODE_TYPE_OR:
			case KETL_BNF_NODE_TYPE_OPTIONAL:
				//break;
			default:
				currentOffset -= deltaOffset;
			}
		}

		switch (solverState->bnfNode->type) {
		case KETL_BNF_NODE_TYPE_REF:
			(*(KETLBnfParserState**)ketlPushOnStack(&parentStack)) = solverState;
			PRINT_SPACE(currentOffset);
			printf("REF\n");
			currentOffset += deltaOffset;
			break;
		case KETL_BNF_NODE_TYPE_CONCAT:
			(*(KETLBnfParserState**)ketlPushOnStack(&parentStack)) = solverState;
			PRINT_SPACE(currentOffset);
			printf("CONCAT\n");
			currentOffset += deltaOffset;
			break;
		case KETL_BNF_NODE_TYPE_OR:
			(*(KETLBnfParserState**)ketlPushOnStack(&parentStack)) = solverState;
			PRINT_SPACE(currentOffset);
			printf("OR\n");
			currentOffset += deltaOffset;
			break;
		case KETL_BNF_NODE_TYPE_OPTIONAL:
			(*(KETLBnfParserState**)ketlPushOnStack(&parentStack)) = solverState;
			PRINT_SPACE(currentOffset);
			printf("OPTIONAL\n");
			currentOffset += deltaOffset;
			break;
		case KETL_BNF_NODE_TYPE_REPEAT:
			(*(KETLBnfParserState**)ketlPushOnStack(&parentStack)) = solverState;
			PRINT_SPACE(currentOffset);
			printf("REPEAT\n");
			currentOffset += deltaOffset;
			break;
		case KETL_BNF_NODE_TYPE_CONSTANT:
			//break;
		default:
			PRINT_SPACE(currentOffset);
			printf("%.*s\n", solverState->token->length - solverState->tokenOffset, solverState->token->value + solverState->tokenOffset);
		}
	}
	ketlDeinitStack(&parentStack);
	ketlResetStackIterator(iterator);
}

void ketlInitSyntaxSolver(KETLSyntaxSolver* syntaxSolver) {
	ketlInitObjectPool(&syntaxSolver->tokenPool, sizeof(KETLToken), 16);
	ketlInitObjectPool(&syntaxSolver->bnfNodePool, sizeof(KETLBnfNode), 16);
	ketlInitStack(&syntaxSolver->bnfStateStack, sizeof(KETLBnfParserState), 32);
	syntaxSolver->bnfScheme = ketlBuildBnfScheme(&syntaxSolver->bnfNodePool);
}

void ketlDeinitSyntaxSolver(KETLSyntaxSolver* syntaxSolver) {
	ketlDeinitStack(&syntaxSolver->bnfStateStack);
	ketlDeinitObjectPool(&syntaxSolver->bnfNodePool);
	ketlDeinitObjectPool(&syntaxSolver->tokenPool);
}

KETLSyntaxNode* ketlSolveSyntax(const char* source, size_t length, KETLSyntaxSolver* syntaxSolver, KETLObjectPool* syntaxNodePool) {
	KETLStack* bnfStateStack = &syntaxSolver->bnfStateStack;
	KETLObjectPool* tokenPool = &syntaxSolver->tokenPool;

	KETLLexer lexer;
	ketlInitLexer(&lexer, source, length, tokenPool);

	if (!ketlHasNextToken(&lexer)) {
		return NULL;
	}
	KETLToken* firstToken = ketlGetNextToken(&lexer);
	KETLToken* token = firstToken;

	while (ketlHasNextToken(&lexer)) {
		token = token->next = ketlGetNextToken(&lexer);
	}
	token->next = NULL;

	{
		KETLBnfParserState* initialSolver = ketlPushOnStack(bnfStateStack);
		initialSolver->bnfNode = syntaxSolver->bnfScheme;
		initialSolver->token = firstToken;
		initialSolver->tokenOffset = 0;

		initialSolver->parent = NULL;
	}

	KETLBnfErrorInfo error;
	{
		error.maxToken = NULL;
		error.maxTokenOffset = 0;
		error.bnfNode = NULL;
	}

	bool success = ketlParseBnf(bnfStateStack, &error);
	if (!success) {
		return NULL;
	}

	KETLStackIterator iterator;
	ketlInitStackIterator(&iterator, bnfStateStack);

	//printBnfSolution(&iterator);

	KETLSyntaxNode* rootSyntaxNode = ketlParseSyntax(syntaxNodePool, &iterator);

	ketlResetStack(bnfStateStack);
	ketlResetPool(tokenPool);
	
	return rootSyntaxNode;
}