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
		case '*':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PROD;
		case '/':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_DIV;
		}
		break;
	}
	case 2: {
		switch (*value) {
		case '=':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_EQUAL;
		case '!':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_UNEQUAL;
		case ':':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_ASSIGN;
		}
		break;
	}
	}
	__debugbreak();
	return 0;
}

static uint32_t calculateNodeLength(KETLSyntaxNode* node) {
	uint32_t length = 0;

	while (node) {
		++length;
		node = node->nextSibling;
	}

	return length;
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

			if (command == NULL) {
				continue;
			}

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
		case KETL_BNF_NODE_TYPE_CONCAT:
			state = ketlIteratorStackGetNext(bnfStackIterator); // '{' or optional
			if (state->bnfNode->type == KETL_BNF_NODE_TYPE_OPTIONAL) {
				// expression
				KETL_ITERATOR_STACK_PEEK(KETLBnfParserState*, next, *bnfStackIterator);

				KETLSyntaxNode* node = NULL;
				if (next->parent == state) {
					ketlIteratorStackSkipNext(bnfStackIterator); // ref
					node = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
				}

				ketlIteratorStackSkipNext(bnfStackIterator); // ;
				return node;
			}
			else {
				// block
				ketlIteratorStackSkipNext(bnfStackIterator); // ref
				KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

				node->type = KETL_SYNTAX_NODE_TYPE_BLOCK;
				node->positionInSource = state->token->positionInSource + state->tokenOffset;
				node->value = state->token->value + state->tokenOffset;
				node->firstChild = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
				node->length = calculateNodeLength(node->firstChild);

				ketlIteratorStackSkipNext(bnfStackIterator); // }

				return node;
			}
		default:
			__debugbreak();
		}
		__debugbreak();
		break;
	}
	case KETL_SYNTAX_BUILDER_TYPE_TYPE:
		state = ketlIteratorStackGetNext(bnfStackIterator); // id or cocncat
		if (state->bnfNode->type == KETL_BNF_NODE_TYPE_ID) {
			KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

			node->type = KETL_SYNTAX_NODE_TYPE_ID;
			node->positionInSource = state->token->positionInSource + state->tokenOffset;
			node->value = state->token->value + state->tokenOffset;
			node->length = state->token->length - state->tokenOffset;

			return node;
		}
		__debugbreak();
		break;
	case KETL_SYNTAX_BUILDER_TYPE_PRIMARY_EXPRESSION:
		state = ketlIteratorStackGetNext(bnfStackIterator);
		switch (state->bnfNode->type) {
		case KETL_BNF_NODE_TYPE_ID: {
			KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

			node->type = KETL_SYNTAX_NODE_TYPE_ID;
			node->positionInSource = state->token->positionInSource + state->tokenOffset;
			node->value = state->token->value + state->tokenOffset;
			node->length = state->token->length - state->tokenOffset;

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
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_2: {
		// LEFT TO RIGHT
		ketlIteratorStackSkipNext(bnfStackIterator); // ref
		KETLSyntaxNode* caller = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
		KETLBnfParserState* state = ketlIteratorStackGetNext(bnfStackIterator); // repeat
		KETL_ITERATOR_STACK_PEEK(KETLBnfParserState*, next, *bnfStackIterator);

		if (next->parent == state) {
			__debugbreak();
		}

		return caller;
	}
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_3: 
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

			KETLBnfParserState* op = ketlIteratorStackGetNext(bnfStackIterator);

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
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_5:
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_6: {
		// RIGHT TO LEFT
		ketlIteratorStackSkipNext(bnfStackIterator); // ref
		KETLSyntaxNode* root = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
		KETLSyntaxNode* left = NULL;

		KETLBnfParserState* repeat = ketlIteratorStackGetNext(bnfStackIterator); // repeat
		KETL_FOREVER{
			KETL_ITERATOR_STACK_PEEK(KETLBnfParserState*, next, *bnfStackIterator);

			if (next->parent != repeat) {
				return root;
			}

			ketlIteratorStackSkipNext(bnfStackIterator); // concat
			ketlIteratorStackSkipNext(bnfStackIterator); // or

			KETLBnfParserState* op = ketlIteratorStackGetNext(bnfStackIterator);

			KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

			node->type = decideOperatorSyntaxType(op->token->value + op->tokenOffset, op->token->length - op->tokenOffset);
			node->positionInSource = op->token->positionInSource + op->tokenOffset;
			node->length = 2;
			if (left != NULL) {
				node->firstChild = left->nextSibling;
				left->nextSibling = node;
				left = node->firstChild;
				node->nextSibling = NULL;
			}
			else {
				node->firstChild = left = root;
				root = node;
			}

			ketlIteratorStackSkipNext(bnfStackIterator); // ref
			KETLSyntaxNode* right = ketlParseSyntax(syntaxNodePool, bnfStackIterator);

			left->nextSibling = right;
			right->nextSibling = NULL;
		}
	}
	case KETL_SYNTAX_BUILDER_TYPE_DEFINE_WITH_ASSIGNMENT: {
		KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

		ketlIteratorStackSkipNext(bnfStackIterator); // or
		state = ketlIteratorStackGetNext(bnfStackIterator); // var or type
		node->positionInSource = state->token->positionInSource + state->tokenOffset;

		KETLSyntaxNode* typeNode = NULL;

		if (state->bnfNode->type == KETL_BNF_NODE_TYPE_CONSTANT) {
			node->length = 2;
			node->type = KETL_SYNTAX_NODE_TYPE_DEFINE_VAR;
		}
		else {
			node->length = 3;
			typeNode = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
			node->firstChild = typeNode;
			node->type = KETL_SYNTAX_NODE_TYPE_DEFINE_VAR_OF_TYPE;
		}

		state = ketlIteratorStackGetNext(bnfStackIterator); // id


		KETLSyntaxNode* idNode = ketlGetFreeObjectFromPool(syntaxNodePool);

		idNode->type = KETL_SYNTAX_NODE_TYPE_ID;
		idNode->positionInSource = state->token->positionInSource + state->tokenOffset;
		idNode->length = state->token->length - state->tokenOffset;
		idNode->value = state->token->value + state->tokenOffset;

		if (typeNode) {
			typeNode->nextSibling = idNode;
		}
		else {
			node->firstChild = idNode;
		}

		ketlIteratorStackSkipNext(bnfStackIterator); // :=
		ketlIteratorStackSkipNext(bnfStackIterator); // ref

		KETLSyntaxNode* expression = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
		expression->nextSibling = NULL;

		idNode->nextSibling = expression;

		ketlIteratorStackSkipNext(bnfStackIterator); // ;
		return node;
	}
	case KETL_SYNTAX_BUILDER_TYPE_IF_ELSE: {
		ketlIteratorStackSkipNext(bnfStackIterator); // if
		ketlIteratorStackSkipNext(bnfStackIterator); // (
		ketlIteratorStackSkipNext(bnfStackIterator); // ref

		KETLSyntaxNode* expression = ketlParseSyntax(syntaxNodePool, bnfStackIterator);

		ketlIteratorStackSkipNext(bnfStackIterator); // )
		ketlIteratorStackSkipNext(bnfStackIterator); // ref

		KETLSyntaxNode* trueBlock = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
		KETLSyntaxNode* falseBlock = NULL;

		KETLBnfParserState* optional = ketlIteratorStackGetNext(bnfStackIterator);

		KETLStackIterator tmpIterator = *bnfStackIterator;
		KETLBnfParserState* next = ketlIteratorStackGetNext(&tmpIterator); // concat
		if (next->parent == optional) {
			*bnfStackIterator = tmpIterator;
			ketlIteratorStackSkipNext(bnfStackIterator); // else
			ketlIteratorStackSkipNext(bnfStackIterator); // ref
			falseBlock = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
		}
		
		KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

		node->firstChild = expression;
		expression->nextSibling = trueBlock;
		trueBlock->nextSibling = falseBlock;

		node->type = KETL_SYNTAX_NODE_TYPE_IF_ELSE;
		node->positionInSource = state->token->positionInSource + state->tokenOffset;

		node->length = 2;
		if (falseBlock != NULL) {
			node->length = 3;
		}

		return node;
	}
	case KETL_SYNTAX_BUILDER_TYPE_RETURN: {
		KETLBnfParserState* returnState = ketlIteratorStackGetNext(bnfStackIterator); // return
		KETLBnfParserState* optional = ketlIteratorStackGetNext(bnfStackIterator); // optional
				
		KETL_ITERATOR_STACK_PEEK(KETLBnfParserState*, next, *bnfStackIterator); // expression

		KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);
		if (next->parent == optional) {
			ketlIteratorStackSkipNext(bnfStackIterator); // ref
			node->firstChild = ketlParseSyntax(syntaxNodePool, bnfStackIterator);
			node->length = 1;
		}
		else {
			node->firstChild = NULL;
			node->length = 0;
		}

		node->type = KETL_SYNTAX_NODE_TYPE_RETURN;
		node->positionInSource = returnState->token->positionInSource + returnState->tokenOffset;

		ketlIteratorStackSkipNext(bnfStackIterator); // ;

		return node;
	}
	case KETL_SYNTAX_BUILDER_TYPE_NONE:
	default:
		__debugbreak();
	}

	return NULL;
}
