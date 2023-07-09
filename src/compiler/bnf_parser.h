//🍲ketl
#ifndef compiler_bnf_parser_h
#define compiler_bnf_parser_h

#include "ketl/utils.h"

#include <stdbool.h>
#include <inttypes.h>

KETL_FORWARD(KETLStack);
KETL_FORWARD(KETLToken);
KETL_FORWARD(KETLBnfNode);

KETL_DEFINE(KETLBnfParserState) {
	KETLBnfNode* bnfNode;
	uint32_t state;
	uint32_t tokenOffset;
	KETLToken* token;
	KETLBnfParserState* parent;
};

KETL_DEFINE(KETLBnfErrorInfo) {
	KETLToken* maxToken;
	uint32_t maxTokenOffset;
	KETLBnfNode* bnfNode;
};

bool ketlParseBnf(KETLStack* syntaxStateStack, KETLBnfErrorInfo* error);

#endif /*compiler_bnf_parser_h*/
