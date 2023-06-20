//🍲ketl
#include "syntax_solver.h"

#include "bnf_parser.h"
#include "syntax_parser.h"

#include "token.h"
#include "syntax_node.h"
#include "bnf_node.h"
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

KETLSyntaxNode* ketlSolveBnf(KETLToken* firstToken, KETLBnfNode* scheme, KETLObjectPool* syntaxNodePool, KETLStack* bnfStateStack) {

	{
		KETLBnfParserState* initialSolver = ketlPushOnStack(bnfStateStack);
		initialSolver->bnfNode = scheme;
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

	KETLStackIterator iterator;
	ketlInitStackIterator(&iterator, bnfStateStack);

	//printBnfSolution(&iterator);

	KETLSyntaxNode* rootSyntaxNode = ketlParseSyntax(syntaxNodePool, &iterator);

	return rootSyntaxNode;
}
