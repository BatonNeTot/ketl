//🍲ketl
#include "bnf_solver.h"

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

KETL_FORWARD(SolverState);

struct SolverState {
	KETLBnfNode* bnfNode;
	uint32_t state;
	uint32_t tokenOffset;
	KETLToken* token;

	SolverState* parent;
	SolverState* firstChild;
	SolverState* nextSibling;
	SolverState* prevSibling;
};

static bool iterateIterator(KETLToken** pToken, uint32_t* pTokenOffset, const char* value, uint32_t valueLength) {
	KETLToken* token = *pToken;
	uint32_t tokenOffset = *pTokenOffset;

	const char* tokenValue = token->value;
	uint32_t tokenLength = token->length;

	uint32_t tokenDiff = tokenLength - tokenOffset;

	if (valueLength > tokenDiff) {
		return false;
	}

	for (uint32_t i = 0u; i < valueLength; ++i) {
		if (value[i] != tokenValue[i + tokenOffset]) {
			return false;
		}
	}

	if (valueLength == tokenDiff) {
		*pToken = token->next;
		*pTokenOffset = 0;
	}
	else {
		*pTokenOffset += valueLength;
	}

	return true;
}

static inline bool iterate(SolverState* solverState, KETLToken** pToken, uint32_t* pTokenOffset) {
	switch (solverState->bnfNode->type) {
	case KETL_BNF_NODE_TYPE_ID: {
		KETLToken* currentToken = *pToken;
		if (currentToken != NULL && currentToken->type == KETL_TOKEN_TYPE_ID) {
			*pToken = currentToken->next;
			return true;
		}
		else {
			return false;
		}
	}
	case KETL_BNF_NODE_TYPE_NUMBER: {
		KETLToken* currentToken = *pToken;
		if (currentToken != NULL && currentToken->type == KETL_TOKEN_TYPE_NUMBER) {
			*pToken = currentToken->next;
			return true;
		}
		else {
			return false;
		}
	}
	case KETL_BNF_NODE_TYPE_STRING: {
		KETLToken* currentToken = *pToken;
		if (currentToken != NULL && currentToken->type == KETL_TOKEN_TYPE_STRING) {
			*pToken = currentToken->next;
			return true;
		}
		else {
			return false;
		}
	}
	case KETL_BNF_NODE_TYPE_CONSTANT:
	case KETL_BNF_NODE_TYPE_SERVICE_CONSTANT: {
		KETLToken* currentToken = *pToken;
		if (currentToken == NULL) {
			return false;
		}

		switch (currentToken->type) {
		case KETL_TOKEN_TYPE_SPECIAL: {
			uint32_t nodeLength = solverState->bnfNode->size;

			if (!iterateIterator(pToken, pTokenOffset, solverState->bnfNode->value, nodeLength)) {
				return false;
			}
			return true;
		}
		case KETL_TOKEN_TYPE_ID: {
			uint32_t tokenLength = currentToken->length;
			uint32_t nodeLength = solverState->bnfNode->size;

			if (!iterateIterator(pToken, pTokenOffset, solverState->bnfNode->value, nodeLength)) {
				return false;
			}
			return true;
		}
		default: {
			return false;
		}
		}
	}
	default: {
		solverState->state = 0;
		return true;
	}
	}
}

static inline KETLBnfNode* nextChild(SolverState* solverState) {
	switch (solverState->bnfNode->type) {
	case KETL_BNF_NODE_TYPE_CONSTANT:
	case KETL_BNF_NODE_TYPE_SERVICE_CONSTANT: {
		return NULL;
	}
	case KETL_BNF_NODE_TYPE_REF: {
		if (solverState->state > 0) {
			return NULL;
		}
		++solverState->state;
		return solverState->bnfNode->ref;
	}
	case KETL_BNF_NODE_TYPE_CONCAT: {
		uint32_t state = solverState->state;
		if (state < solverState->bnfNode->size) {
			++solverState->state;
			KETLBnfNode* it = solverState->bnfNode->firstChild;
			for (uint32_t i = 0; i < state; ++i) {
				it = it->nextSibling;
			}
			return it;
		}
		else {
			return NULL;
		}
	}
	case KETL_BNF_NODE_TYPE_OR:
	case KETL_BNF_NODE_TYPE_OPTIONAL: {
		uint32_t size = solverState->bnfNode->size;
		uint32_t state = solverState->state;
		if (state >= size) {
			return NULL;
		}
		solverState->state += size;
		KETLBnfNode* it = solverState->bnfNode->firstChild;
		for (uint32_t i = 0; i < state; ++i) {
			it = it->nextSibling;
		}
		return it;
	}
	case KETL_BNF_NODE_TYPE_REPEAT: {
		if (solverState->state == 1) {
			solverState->state = 0;
			return NULL;
		}
		return solverState->bnfNode->firstChild;
	}
	default:
		return NULL;
	}
}

static inline bool childRejected(SolverState* solverState) {
	switch (solverState->bnfNode->type) {
	case KETL_BNF_NODE_TYPE_CONCAT: {
		--solverState->state;
		return false;
	}
	case KETL_BNF_NODE_TYPE_OR: {
		uint32_t size = solverState->bnfNode->size;
		return (solverState->state -= size - 1) < size;
	}
	case KETL_BNF_NODE_TYPE_OPTIONAL: {
		uint32_t size = solverState->bnfNode->size;
		return (solverState->state -= size - 1) <= size;
	}
	case KETL_BNF_NODE_TYPE_REPEAT: {
		solverState->state = 1;
		return true;
	}
	default:
		return false;
	}
}

typedef struct ErrorInfo {
	KETLToken* maxToken;
	uint32_t maxTokenOffset;
	KETLBnfNode* bnfNode;
} ErrorInfo;

static bool solveBnfImpl(KETLToken* firstToken, KETLBnfNode* scheme, KETLStack* syntaxStateStack, ErrorInfo* error) {
	{
		SolverState* initialSolver = ketlPushOnStack(syntaxStateStack);
		initialSolver->bnfNode = scheme;
		initialSolver->token = firstToken;
		initialSolver->tokenOffset = 0;

		initialSolver->parent = NULL;
		initialSolver->firstChild = NULL;
		initialSolver->nextSibling = NULL;
		initialSolver->prevSibling = NULL;
	}

	bool success = false;

	while (!ketlIsStackEmpty(syntaxStateStack)) {
		SolverState* current = ketlPeekStack(syntaxStateStack);

		KETLToken* currentToken = current->token;
		uint32_t currentTokenOffset = current->tokenOffset;

		if (!iterate(current, &currentToken, &currentTokenOffset)) {
			if (error->maxToken == currentToken && error->maxTokenOffset == currentTokenOffset) {
				error->bnfNode = current->bnfNode;
			}
			else if (error->maxToken == NULL || currentToken == NULL ||
				error->maxToken->positionInSource + error->maxTokenOffset
				< currentToken->positionInSource + currentTokenOffset) {
				error->maxToken = currentToken;
				error->maxTokenOffset = currentTokenOffset;
				error->bnfNode = current->bnfNode;
			}
			KETL_FOREVER {
				ketlPopStack(syntaxStateStack);
				SolverState* parent = current->parent;
				if (parent && childRejected(parent)) {
					current = parent;
					break;
				}
				if (ketlIsStackEmpty(syntaxStateStack)) {
					return (currentToken == NULL);
				}
				current = ketlPeekStack(syntaxStateStack);
			}
		}

		KETLBnfNode* next;
		KETL_FOREVER {
			next = nextChild(current);

			if (next != NULL) {
				SolverState* pushed = ketlPushOnStack(syntaxStateStack);

				pushed->bnfNode = next;
				pushed->token = currentToken;
				pushed->tokenOffset = currentTokenOffset;

				pushed->parent = current;
				pushed->firstChild = NULL;
				pushed->nextSibling = NULL;
				pushed->prevSibling = NULL;
				break;
			}

			current = current->parent;
			if (current == NULL) {
				return (currentToken == NULL);
			}
		}
	}

	return false;
}

KETLSyntaxNode* ketlSolveBnf(KETLToken* firstToken, KETLBnfNode* scheme, KETLObjectPool* syntaxNodePool) {
	KETLStack syntaxStateStack;
	ketlInitStack(&syntaxStateStack, sizeof(SolverState), 32);

	ErrorInfo error;

	error.maxToken = NULL;
	error.maxTokenOffset = 0;
	error.bnfNode = NULL;

	bool success = solveBnfImpl(firstToken, scheme, &syntaxStateStack, &error);

	ketlDeinitStack(&syntaxStateStack);

	return example(syntaxNodePool); //NULL;
}
