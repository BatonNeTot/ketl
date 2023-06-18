//🍲ketl
#ifndef compiler_bnf_node_h
#define compiler_bnf_node_h

#include "ketl/common.h"

#include <inttypes.h>

typedef uint8_t KETLBnfNodeType;

#define KETL_BNF_NODE_TYPE_ID 0
#define KETL_BNF_NODE_TYPE_NUMBER 1
#define KETL_BNF_NODE_TYPE_STRING 2
#define KETL_BNF_NODE_TYPE_CONSTANT 3
#define KETL_BNF_NODE_TYPE_SERVICE_CONSTANT 4
#define KETL_BNF_NODE_TYPE_REF 5
#define KETL_BNF_NODE_TYPE_CONCAT 6
#define KETL_BNF_NODE_TYPE_OR 7
#define KETL_BNF_NODE_TYPE_OPTIONAL 8
#define KETL_BNF_NODE_TYPE_REPEAT 9

KETL_FORWARD(KETLBnfNode);

struct KETLBnfNode {
	uint32_t size;
	KETLBnfNodeType type;
	KETLBnfNode* nextSibling;
	union {
		const char* value;
		KETLBnfNode* firstChild;
		KETLBnfNode* ref;
	};
};

#endif /*compiler_bnf_node_h*/
