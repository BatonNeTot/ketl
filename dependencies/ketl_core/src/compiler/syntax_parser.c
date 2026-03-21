//🍲ketl
#include "syntax_parser.h"

#include "token.h"
#include "ketl/compiler/syntax_node.h"
#include "bnf_node.h"
#include "bnf_parser.h"
#include "ketl/stack.h"
#include "ketl/object_pool.h"

void ketl_count_lines(const char* source, uint64_t length, uint32_t* pLine, uint32_t* pColumn) {
	uint64_t line = 0;
	uint64_t column = 0;

	for (uint64_t i = 0u; i < length; ++i) {
		if (source[i] == '\n') {
			++line;
			column = 0;
		} else {
			++column;
		}
	}

	*pLine = line;
	*pColumn = column;
}

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
	KETL_DEBUGBREAK();
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

KETLSyntaxNode* ketlParseSyntax(KETLObjectPool* syntaxNodePool, KETLStackIterator* bnfStackIterator, const char* source) {
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
			KETLSyntaxNode* command = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);

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
			return ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
		case KETL_BNF_NODE_TYPE_CONCAT:
			state = ketlIteratorStackGetNext(bnfStackIterator); // '{' or optional
			if (state->bnfNode->type == KETL_BNF_NODE_TYPE_OPTIONAL) {
				// expression
				KETL_ITERATOR_STACK_PEEK(KETLBnfParserState*, next, *bnfStackIterator);

				KETLSyntaxNode* node = NULL;
				if (next->parent == state) {
					ketlIteratorStackSkipNext(bnfStackIterator); // ref
					node = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
				}

				ketlIteratorStackSkipNext(bnfStackIterator); // ;
				return node;
			}
			else {
				// block
				ketlIteratorStackSkipNext(bnfStackIterator); // ref
				KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

				node->type = KETL_SYNTAX_NODE_TYPE_BLOCK;
				ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
					&node->lineInSource, &node->columnInSource);
				node->value = state->token->value + state->tokenOffset;
				node->firstChild = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
				node->length = calculateNodeLength(node->firstChild);

				ketlIteratorStackSkipNext(bnfStackIterator); // }

				return node;
			}
		default:
			KETL_DEBUGBREAK();
		}
		KETL_DEBUGBREAK();
		break;
	}
	case KETL_SYNTAX_BUILDER_TYPE_TYPE:
		state = ketlIteratorStackGetNext(bnfStackIterator); // id or cocncat
		if (state->bnfNode->type == KETL_BNF_NODE_TYPE_ID) {
			KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

			node->type = KETL_SYNTAX_NODE_TYPE_ID;
			ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
				&node->lineInSource, &node->columnInSource);
			node->value = state->token->value + state->tokenOffset;
			node->length = state->token->length - state->tokenOffset;

			return node;
		}
		KETL_DEBUGBREAK();
		break;
	case KETL_SYNTAX_BUILDER_TYPE_PRIMARY_EXPRESSION:
		state = ketlIteratorStackGetNext(bnfStackIterator);
		switch (state->bnfNode->type) {
		case KETL_BNF_NODE_TYPE_ID: {
			KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

			node->type = KETL_SYNTAX_NODE_TYPE_ID;
			ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
				&node->lineInSource, &node->columnInSource);
			node->value = state->token->value + state->tokenOffset;
			node->length = state->token->length - state->tokenOffset;

			return node;
		}
		case KETL_BNF_NODE_TYPE_NUMBER: {
			KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

			node->type = KETL_SYNTAX_NODE_TYPE_NUMBER;
			ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
				&node->lineInSource, &node->columnInSource);
			node->value = state->token->value + state->tokenOffset;
			node->length = state->token->length - state->tokenOffset;

			return node;
		}
		case KETL_BNF_NODE_TYPE_CONCAT: {
			ketlIteratorStackSkipNext(bnfStackIterator); // (
			ketlIteratorStackSkipNext(bnfStackIterator); // ref
			KETLSyntaxNode* node = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
			ketlIteratorStackSkipNext(bnfStackIterator); // )
			return node;
		}
		default:
			KETL_DEBUGBREAK();
		}
		break;
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_1: {
		// LEFT TO RIGHT
		ketlIteratorStackSkipNext(bnfStackIterator); // ref
		KETLSyntaxNode* left = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
		KETLBnfParserState* state = ketlIteratorStackGetNext(bnfStackIterator); // repeat
		KETL_ITERATOR_STACK_PEEK(KETLBnfParserState*, next, *bnfStackIterator);

		if (next->parent == state) {
			KETL_DEBUGBREAK();
		}

		return left;
	}
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_2: {
		// LEFT TO RIGHT
		ketlIteratorStackSkipNext(bnfStackIterator); // ref
		KETLSyntaxNode* caller = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
		KETLBnfParserState* state = ketlIteratorStackGetNext(bnfStackIterator); // repeat
		KETL_ITERATOR_STACK_PEEK(KETLBnfParserState*, next, *bnfStackIterator);

		if (next->parent == state) {
			KETL_DEBUGBREAK();
		}

		return caller;
	}
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_3: 
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_4: {
		// LEFT TO RIGHT
		ketlIteratorStackSkipNext(bnfStackIterator); // ref
		KETLSyntaxNode* left = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);

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
			ketl_count_lines(source, op->token->positionInSource + op->tokenOffset, 
				&node->lineInSource, &node->columnInSource);
			node->length = 2;
			node->firstChild = left;

			ketlIteratorStackSkipNext(bnfStackIterator); // ref
			KETLSyntaxNode* right = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);

			left->nextSibling = right;
			right->nextSibling = NULL;

			left = node;
		}
	}
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_5:
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_6: {
		// RIGHT TO LEFT
		ketlIteratorStackSkipNext(bnfStackIterator); // ref
		KETLSyntaxNode* root = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
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
			ketl_count_lines(source, op->token->positionInSource + op->tokenOffset, 
				&node->lineInSource, &node->columnInSource);
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
			KETLSyntaxNode* right = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);

			left->nextSibling = right;
			right->nextSibling = NULL;
		}
	}
	case KETL_SYNTAX_BUILDER_TYPE_DEFINE_WITH_ASSIGNMENT: {
		KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

		ketlIteratorStackSkipNext(bnfStackIterator); // or
		state = ketlIteratorStackGetNext(bnfStackIterator); // var or type
		ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
			&node->lineInSource, &node->columnInSource);

		KETLSyntaxNode* typeNode = NULL;

		if (state->bnfNode->type == KETL_BNF_NODE_TYPE_CONSTANT) {
			node->length = 2;
			node->type = KETL_SYNTAX_NODE_TYPE_DEFINE_VAR;
		}
		else {
			node->length = 3;
			typeNode = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
			node->firstChild = typeNode;
			node->type = KETL_SYNTAX_NODE_TYPE_DEFINE_VAR_OF_TYPE;
		}

		state = ketlIteratorStackGetNext(bnfStackIterator); // id


		KETLSyntaxNode* idNode = ketlGetFreeObjectFromPool(syntaxNodePool);

		idNode->type = KETL_SYNTAX_NODE_TYPE_ID;
		ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
			&idNode->lineInSource, &idNode->columnInSource);
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

		KETLSyntaxNode* expression = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
		expression->nextSibling = NULL;

		idNode->nextSibling = expression;

		ketlIteratorStackSkipNext(bnfStackIterator); // ;
		return node;
	}
	case KETL_SYNTAX_BUILDER_TYPE_IF_ELSE: {
		ketlIteratorStackSkipNext(bnfStackIterator); // if
		ketlIteratorStackSkipNext(bnfStackIterator); // (
		ketlIteratorStackSkipNext(bnfStackIterator); // ref

		KETLSyntaxNode* expression = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);

		ketlIteratorStackSkipNext(bnfStackIterator); // )
		ketlIteratorStackSkipNext(bnfStackIterator); // ref

		KETLSyntaxNode* trueBlock = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
		KETLSyntaxNode* falseBlock = NULL;

		KETLBnfParserState* optional = ketlIteratorStackGetNext(bnfStackIterator);

		KETLStackIterator tmpIterator = *bnfStackIterator;
		KETLBnfParserState* next = ketlIteratorStackGetNext(&tmpIterator); // concat
		if (next->parent == optional) {
			*bnfStackIterator = tmpIterator;
			ketlIteratorStackSkipNext(bnfStackIterator); // else
			ketlIteratorStackSkipNext(bnfStackIterator); // ref
			falseBlock = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
			falseBlock->nextSibling = NULL;
		}
		
		KETLSyntaxNode* node = ketlGetFreeObjectFromPool(syntaxNodePool);

		node->firstChild = expression;
		expression->nextSibling = trueBlock;
		trueBlock->nextSibling = falseBlock;

		node->type = KETL_SYNTAX_NODE_TYPE_IF_ELSE;
		ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
			&node->lineInSource, &node->columnInSource);

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
			KETLSyntaxNode* expression = ketlParseSyntax(syntaxNodePool, bnfStackIterator, source);
			expression->nextSibling = NULL;
			node->firstChild = expression;
			node->length = 1;
		}
		else {
			node->firstChild = NULL;
			node->length = 0;
		}

		node->type = KETL_SYNTAX_NODE_TYPE_RETURN;
		ketl_count_lines(source, returnState->token->positionInSource + returnState->tokenOffset, 
			&node->lineInSource, &node->columnInSource);

		ketlIteratorStackSkipNext(bnfStackIterator); // ;

		return node;
	}
	case KETL_SYNTAX_BUILDER_TYPE_NONE:
	default:
		KETL_DEBUGBREAK();
	}

	return NULL;
}
