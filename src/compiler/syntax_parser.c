//🍲ketl
#include "syntax_parser.h"

#include "token.h"
#include "syntax_node.h"
#include "bnf_node.h"
#include "bnf_parser.h"
#include "ketl/stack.h"
#include "ketl/object_pool.h"

#include <debugapi.h>

static KETLSyntaxNodeType decideOperatorSyntaxType(const char* value, uint32_t length) {
	switch (length) {
	case 1: {
		switch (*value) {
		case '+':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS;
		case '-':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS;
		}
		break;
	}
	default:
		__debugbreak();
	}
}

KETLSyntaxNode* ketlParseSyntax(KETLObjectPool* syntaxNodePool, KETLStackIterator* bnfStackIterator) {
	KETLBnfParserState* state = ketlIteratorStackGetNext(bnfStackIterator);

	switch (state->bnfNode->builder) {
	case KETL_SYNTAX_BUILDER_TYPE_BLOCK: {
		KETLSyntaxNode* first = NULL;
		KETLSyntaxNode* last = NULL;

		KETL_FOREVER{ 
			if (!ketlIteratorStackHasNext(bnfStackIterator)) {
				break;
			}

			KETL_ITERATOR_STACK_PEEK(KETLBnfParserState*, next, *bnfStackIterator);

			if (next->parent != state) {
				break;
			}

			ketlIteratorStackSkipNext(bnfStackIterator); // ref
			KETLSyntaxNode* command = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
			if (first == NULL) {
				first = command;
				last = command;
			}
			else {
				last->nextSibling = command;
				last = command;
			}
		}

		if (last != NULL) {
			last->nextSibling = NULL;
		}
		return first;
	}
	case KETL_SYNTAX_BUILDER_TYPE_COMMAND: {
		state = ketlIteratorStackGetNext(bnfStackIterator);
		switch (state->bnfNode->type) {
		case KETL_BNF_NODE_TYPE_REF:
			return ketlParseSyntax(syntaxNodePool, bnfStackIterator);
		default:
			__debugbreak();
		}
		__debugbreak();
		break;
	}
	case KETL_SYNTAX_BUILDER_TYPE_PRIMARY_EXPRESSION:
		state = ketlIteratorStackGetNext(bnfStackIterator);
		switch (state->bnfNode->type) {
		case KETL_BNF_NODE_TYPE_ID: {
			KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

			node->type = KETL_SYNTAX_NODE_TYPE_ID;
			node->positionInSource = state->token->positionInSource + state->tokenOffset;
			node->value = state->token->value + state->tokenOffset;
			node->length = state->token->length - state->tokenOffset;
			node->firstChild = NULL;

			return node;
		}
		case KETL_BNF_NODE_TYPE_NUMBER: {
			KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

			node->type = KETL_SYNTAX_NODE_TYPE_NUMBER;
			node->positionInSource = state->token->positionInSource + state->tokenOffset;
			node->value = state->token->value + state->tokenOffset;
			node->length = state->token->length - state->tokenOffset;

			return node;
		}
		default:
			__debugbreak();
		}
		__debugbreak();
		break;
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_1: {
		// LEFT TO RIGHT
		ketlIteratorStackSkipNext(bnfStackIterator); // ref
		KETLSyntaxNode* left = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
		KETLBnfParserState* state = ketlIteratorStackGetNext(bnfStackIterator); // repeat
		KETL_ITERATOR_STACK_PEEK(KETLBnfParserState*, next, *bnfStackIterator);

		if (next->parent == state) {
			__debugbreak();
		}

		return left;
	}
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_4: {
		// LEFT TO RIGHT
		ketlIteratorStackSkipNext(bnfStackIterator); // ref
		KETLSyntaxNode* left = ketlParseSyntax(syntaxNodePool, bnfStackIterator);

		KETLBnfParserState* repeat = ketlIteratorStackGetNext(bnfStackIterator); // repeat
		KETL_FOREVER {
			KETL_ITERATOR_STACK_PEEK(KETLBnfParserState*, next, *bnfStackIterator);

			if (next->parent != repeat) {
				return left;
			}

			ketlIteratorStackSkipNext(bnfStackIterator); // concat
			ketlIteratorStackSkipNext(bnfStackIterator); // or

			KETLBnfParserState* op = ketlIteratorStackGetNext(bnfStackIterator); // repeat

			KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

			node->type = decideOperatorSyntaxType(op->token->value + op->tokenOffset, op->token->length - op->tokenOffset);
			node->positionInSource = op->token->positionInSource + op->tokenOffset;
			node->length = 2;
			node->firstChild = left;

			ketlIteratorStackSkipNext(bnfStackIterator); // ref
			KETLSyntaxNode* right = ketlParseSyntax(syntaxNodePool, bnfStackIterator);

			left->nextSibling = right;
			right->nextSibling = NULL;

			left = node;
		}
	}
	case KETL_SYNTAX_BUILDER_TYPE_DEFINE_WITH_ASSIGNMENT: {
		KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);
		node->type = KETL_SYNTAX_NODE_TYPE_DEFINE_VAR;

		state = ketlIteratorStackGetNext(bnfStackIterator); // let

		node->positionInSource = state->token->positionInSource + state->tokenOffset;

		state = ketlIteratorStackGetNext(bnfStackIterator); // id

		node->length = 2;

		KETLSyntaxNode* idNode = ketlGetFreeObjectFromPool(syntaxNodePool);

		idNode->type = KETL_SYNTAX_NODE_TYPE_ID;
		idNode->positionInSource = state->token->positionInSource + state->tokenOffset;
		idNode->length = state->token->length - state->tokenOffset;
		idNode->value = state->token->value + state->tokenOffset;

		node->firstChild = idNode;

		ketlIteratorStackSkipNext(bnfStackIterator); // :=
		ketlIteratorStackSkipNext(bnfStackIterator); // ref

		KETLSyntaxNode* expression = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
		expression->nextSibling = NULL;

		idNode->nextSibling = expression;

		ketlIteratorStackSkipNext(bnfStackIterator); // ;
		return node;
	}
	case KETL_SYNTAX_BUILDER_TYPE_NONE:
	default:
		__debugbreak();
	}

	return NULL;
}
