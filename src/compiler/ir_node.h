//🍲ketl
#ifndef compiler_ir_node_h
#define compiler_ir_node_h

#include "ketl/utils.h"

#include <inttypes.h>

typedef uint8_t KETLIRExpressionType;

KETL_FORWARD(KETLIRExpression);
KETL_FORWARD(KETLType);

struct KETLIRExpression {
	KETLIRExpressionType type;
	KETLType* valueType;
	void* calculatedvValue;
};

typedef uint8_t KETLIRCommandType;

#define KETL_IR_COMMAND_TYPE_DEFINE_VAR 0

KETL_FORWARD(KETLIRCommand);

struct KETLIRCommand {
	KETLIRCommandType type;
	KETLIRExpression* expression;
	KETLIRCommand* next;
	KETLIRCommand* additionalNext;
};

#endif /*compiler_ir_node_h*/
