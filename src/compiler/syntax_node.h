//🍲ketl
#ifndef compiler_syntax_node_h
#define compiler_syntax_node_h

#include "ketl/utils.h"

#include <inttypes.h>

typedef uint8_t KETLSyntaxNodeType;

#define KETL_SYNTAX_NODE_TYPE_BLOCK 0
#define KETL_SYNTAX_NODE_TYPE_SIMPLE_EXPRESSION 1
#define KETL_SYNTAX_NODE_TYPE_DEFINE_VAR 2
#define KETL_SYNTAX_NODE_TYPE_DEFINE_VAR_OF_TYPE 3

#define KETL_SYNTAX_NODE_TYPE_ID 4
#define KETL_SYNTAX_NODE_TYPE_NUMBER 5
#define KETL_SYNTAX_NODE_TYPE_STRING_LITERAL 6

#define KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS 7
#define KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS 8
#define KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_ASSIGN 9

#define KETL_SYNTAX_NODE_TYPE_RETURN 10

KETL_DEFINE(KETLSyntaxNode) {
	ptrdiff_t positionInSource;
	uint32_t length;
	KETLSyntaxNodeType type;
	KETLSyntaxNode* nextSibling;
	union {
		const char* value;
		KETLSyntaxNode* firstChild;
	};
};

#endif /*compiler_syntax_node_h*/
