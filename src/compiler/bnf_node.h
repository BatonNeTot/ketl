//🍲ketl
#ifndef compiler_bnf_node_h
#define compiler_bnf_node_h

#include "ketl/common.h"

#include <inttypes.h>

typedef uint8_t KETLBnfNodeType;

#define KETL_BNF_NODE_TYPE_ID 0
#define KETL_BNF_NODE_TYPE_NUMBER 0
#define KETL_BNF_NODE_TYPE_LITERAL 0
#define KETL_BNF_NODE_TYPE_SERVICE_LITERAL 0
#define KETL_BNF_NODE_TYPE_REF 0
#define KETL_BNF_NODE_TYPE_CONCAT 0
#define KETL_BNF_NODE_TYPE_OR 0
#define KETL_BNF_NODE_TYPE_OPTIONAL 0
#define KETL_BNF_NODE_TYPE_REPEAT 0

KETL_FORWARD(KETLBnfNode);

struct KETLBnfNode {
	KETLBnfNodeType type;
	KETLBnfNode* nextSibling;
	union {
		const char* value;
		KETLBnfNode* firstChild;
		KETLBnfNode* ref;
	};
};

#endif /*compiler_bnf_node_h*/
