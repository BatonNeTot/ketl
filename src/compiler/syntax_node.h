//🍲ketl
#ifndef compiler_syntax_node_h
#define compiler_syntax_node_h

#include "ketl/common.h"

#include <inttypes.h>

typedef uint8_t KETLSyntaxNodeType;

#define KETL_SYNTAX_NODE_TYPE_BLOCK 0
#define KETL_SYNTAX_NODE_TYPE_OPERATOR 1
#define KETL_SYNTAX_NODE_TYPE_ID 2
#define KETL_SYNTAX_NODE_TYPE_NUMBER 3
#define KETL_SYNTAX_NODE_TYPE_STRING_LITERAL 4
#define KETL_SYNTAX_NODE_TYPE_DEFINE_VAR 5
#define KETL_SYNTAX_NODE_TYPE_DEFINE_VAR_OF_TYPE 6

KETL_FORWARD(KETLSyntaxNode);

struct KETLSyntaxNode {
	ptrdiff_t positionInSource;
	const char* value;
	uint16_t length;
	KETLSyntaxNodeType type;
	KETLSyntaxNode* firstChild;
	KETLSyntaxNode* nextSibling;
};

#endif /*compiler_syntax_node_h*/
