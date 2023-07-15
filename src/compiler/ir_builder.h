//🍲ketl
#ifndef compiler_ir_builder_h
#define compiler_ir_builder_h

#include "ketl/utils.h"

#include "ketl/object_pool.h"
#include "ketl/int_map.h"

KETL_FORWARD(KETLIRState);
KETL_FORWARD(KETLIRInstruction);
KETL_FORWARD(KETLIRValue);
KETL_FORWARD(KETLSyntaxNode);
KETL_FORWARD(KETLAtomicStrings);
KETL_FORWARD(KETLType);

KETL_DEFINE(KETLIRBuilder) {
	KETLIntMap variables;
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

	uint64_t scopeIndex;
	// TODO namespace
};

void ketlInitIRBuilder(KETLIRBuilder* irBuilder);

void ketlDeinitIRBuilder(KETLIRBuilder* irBuilder);

void ketlBuildIR(KETLType* returnType, KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNodeRoot, KETLAtomicStrings* strings);

#endif /*compiler_ir_builder_h*/
