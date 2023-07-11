//🍲ketl
#ifndef compiler_ir_builder_h
#define compiler_ir_builder_h

#include "ketl/utils.h"

#include "ketl/object_pool.h"

KETL_FORWARD(KETLIRState);
KETL_FORWARD(KETLIRInstruction);
KETL_FORWARD(KETLIRValue);
KETL_FORWARD(KETLSyntaxNode);

KETL_DEFINE(KETLIRBuilder) {
	KETLObjectPool irInstructionPool;
	KETLObjectPool udelegatePool;
	KETLObjectPool uvaluePool;
	KETLObjectPool valuePool;
};

KETL_DEFINE(KETLIRState) {
	KETLIRInstruction* first;
	KETLIRInstruction* last;

	union {
		struct {
			KETLIRValue* stackRoot;
			KETLIRValue* currentStack;
		};
		struct {
			KETLIRValue* tempVariables;
			KETLIRValue* localVariables;
		};
	};

	// TODO namespace
};

void ketlInitIRBuilder(KETLIRBuilder* irBuilder);

void ketlDeinitIRBuilder(KETLIRBuilder* irBuilder);

void ketlBuildIR(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNodeRoot);

#endif /*compiler_ir_builder_h*/
