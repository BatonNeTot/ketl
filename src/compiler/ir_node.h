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
	KETLInstructionArgumentTraits argTraits;
	KETLIRValue* parent;
	KETLIRValue* nextSibling;
	KETLIRValue* firstChild;
};

KETL_DEFINE(KETLIRInstruction) {
	KETLInstructionCode code;
	KETLIRValue* arguments[KETL_INSTRUCTION_MAX_ARGUMENTS_COUNT];
	uint64_t instructionOffset;
	KETLIRInstruction* next;
};

#endif /*compiler_ir_node_h*/
