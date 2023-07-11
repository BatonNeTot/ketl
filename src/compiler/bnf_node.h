//🍲ketl
#ifndef compiler_bnf_node_h
#define compiler_bnf_node_h

#include "ketl/utils.h"

#include <inttypes.h>

typedef uint8_t KETLBnfNodeType;

#define KETL_BNF_NODE_TYPE_ID 0
#define KETL_BNF_NODE_TYPE_NUMBER 1
#define KETL_BNF_NODE_TYPE_STRING 2
#define KETL_BNF_NODE_TYPE_CONSTANT 3
#define KETL_BNF_NODE_TYPE_REF 4
#define KETL_BNF_NODE_TYPE_CONCAT 5
#define KETL_BNF_NODE_TYPE_OR 6
#define KETL_BNF_NODE_TYPE_OPTIONAL 7
#define KETL_BNF_NODE_TYPE_REPEAT 8

typedef uint8_t KETLSyntaxBuilderType;

#define KETL_SYNTAX_BUILDER_TYPE_NONE 0
#define KETL_SYNTAX_BUILDER_TYPE_BLOCK 1
#define KETL_SYNTAX_BUILDER_TYPE_COMMAND 2
#define KETL_SYNTAX_BUILDER_TYPE_PRIMARY_EXPRESSION 3
#define KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_1 4
#define KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_4 5
#define KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_6 6
#define KETL_SYNTAX_BUILDER_TYPE_DEFINE_WITH_ASSIGNMENT 7

KETL_DEFINE(KETLBnfNode) {
	uint32_t size;
	KETLBnfNodeType type;
	KETLSyntaxBuilderType builder;
	KETLBnfNode* nextSibling;
	union {
		const char* value;
		KETLBnfNode* firstChild;
		KETLBnfNode* ref;
	};
};

#endif /*compiler_bnf_node_h*/
