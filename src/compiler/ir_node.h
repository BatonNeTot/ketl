//🍲ketl
#ifndef compiler_ir_node_h
#define compiler_ir_node_h

#include "ketl/instructions.h"
#include "ketl/type.h"
#include "ketl/utils.h"

KETL_FORWARD(KETLType);

KETL_DEFINE(KETLIRValue) {
	const char* name;
	KETLType* type;
	KETLVariableTraits traits;
	KETLInstructionArgument argument;
	KETLInstructionArgumentType argType;
	KETLIRValue* parent;
	KETLIRValue* nextSibling;
	KETLIRValue* firstChild;
};

KETL_DEFINE(KETLIRInstruction) {
	KETLInstructionCode code;
	KETLIRValue* arguments[7];
	KETLIRInstruction* next;
};

#endif /*compiler_ir_node_h*/
