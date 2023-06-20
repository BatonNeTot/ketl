//🍲ketl
#include "syntax_solver.h"

#include "bnf_parser.h"
#include "syntax_parser.h"

#include "token.h"
#include "syntax_node.h"
#include "bnf_node.h"
#include "ketl/object_pool.h"
#include "ketl/stack.h"

static inline KETLSyntaxNode* example(KETLObjectPool* syntaxNodePool) {
	KETLSyntaxNode* define1 = ketlGetFreeObjectFromPool(syntaxNodePool);
	define1->type = KETL_SYNTAX_NODE_TYPE_DEFINE_VAR;
	define1->value = "test1";
	define1->length = 5;

	KETLSyntaxNode* plus = ketlGetFreeObjectFromPool(syntaxNodePool);
	plus->type = KETL_SYNTAX_NODE_TYPE_OPERATOR;
	plus->value = "+";
	plus->length = 1;
	plus->nextSibling = NULL;
	define1->firstChild = plus;

	KETLSyntaxNode* number1 = ketlGetFreeObjectFromPool(syntaxNodePool);
	number1->type = KETL_SYNTAX_NODE_TYPE_NUMBER;
	number1->value = "5";
	number1->length = 1;
	number1->firstChild = NULL;
	plus->firstChild = number1;

	KETLSyntaxNode* number2 = ketlGetFreeObjectFromPool(syntaxNodePool);
	number2->type = KETL_SYNTAX_NODE_TYPE_NUMBER;
	number2->value = "10";
	number2->length = 2;
	number2->firstChild = NULL;
	number2->nextSibling = NULL;
	number1->nextSibling = number2;

	KETLSyntaxNode* define2 = ketlGetFreeObjectFromPool(syntaxNodePool);
	define2->type = KETL_SYNTAX_NODE_TYPE_DEFINE_VAR;
	define2->value = "test2";
	define2->length = 5;
	define2->nextSibling = NULL;
	define1->nextSibling = define2;

	KETLSyntaxNode* minus = ketlGetFreeObjectFromPool(syntaxNodePool);
	minus->type = KETL_SYNTAX_NODE_TYPE_OPERATOR;
	minus->value = "-";
	minus->length = 1;
	minus->nextSibling = NULL;
	define2->firstChild = minus;

	KETLSyntaxNode* id = ketlGetFreeObjectFromPool(syntaxNodePool);
	id->type = KETL_SYNTAX_NODE_TYPE_ID;
	id->value = "test1";
	id->length = 5;
	id->firstChild = NULL;
	minus->firstChild = id;

	KETLSyntaxNode* number3 = ketlGetFreeObjectFromPool(syntaxNodePool);
	number3->type = KETL_SYNTAX_NODE_TYPE_NUMBER;
	number3->value = "17";
	number3->length = 2;
	number3->firstChild = NULL;
	number3->nextSibling = NULL;
	id->nextSibling = number3;

	return define1;
}

#define PRINT_SPACE(x) printf("%*s", (x), " ")

KETLSyntaxNode* ketlSolveBnf(KETLToken* firstToken, KETLBnfNode* scheme, KETLObjectPool* syntaxNodePool) {
	KETLStack syntaxStateStack;
	ketlInitStack(&syntaxStateStack, sizeof(KETLBnfParserState), 32);

	{
		KETLBnfParserState* initialSolver = ketlPushOnStack(&syntaxStateStack);
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

	bool success = ketlParseBnf(&syntaxStateStack, &error);

	KETLStack parentStack;
	ketlInitStack(&parentStack, sizeof(void*), 16);
	KETLStackIterator iterator;
	ketlInitStackIterator(&iterator, &syntaxStateStack);

	int deltaOffset = 4;
	int currentOffset = 0;
	while (ketlIteratorStackHasNext(&iterator)) {
		KETLBnfParserState* solverState = ketlIteratorStackGetNext(&iterator);

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

	ketlResetStack(&parentStack, sizeof(void*), 16);
	ketlInitStackIterator(&iterator, &syntaxStateStack);
	KETLSyntaxNode* rootSyntaxNode = ketlParseSyntax(syntaxNodePool, &iterator, &parentStack);

	ketlDeinitStack(&parentStack);
	ketlDeinitStack(&syntaxStateStack);

	return rootSyntaxNode;
}
