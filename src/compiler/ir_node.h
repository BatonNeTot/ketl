//🍲ketl
#ifndef compiler_ir_node_h
#define compiler_ir_node_h

#include "ketl/instructions.h"
#include "ketl/utils.h"

KETL_FORWARD(KETLIRValue);

KETL_DEFINE(KETLIRInstruction) {
	KETLInstructionCode code;
	KETLIRValue* arguments[7];
	KETLIRInstruction* next;
};

#endif /*compiler_ir_node_h*/
