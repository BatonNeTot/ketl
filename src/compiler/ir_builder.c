//🍲ketl
#include "ir_builder.h"

#include "syntax_node.h"
#include "ir_node.h"

#include "ketl/object_pool.h"
#include "ketl/assert.h"

KETL_FORWARD(KETLType);

KETL_DEFINE(KETLIRValue) {
	KETLType* type;
	KETLInstructionArgument argument;
	KETLInstructionArgumentType argType;
	KETLIRValue* nextOnStack;
};

KETL_DEFINE(IRUndefinedValue) {
	KETLIRValue* value;
	IRUndefinedValue* next;
};

typedef uint8_t DELEGATE_TYPE;

#define DELEGATE_TYPE_NONE 0
#define DELEGATE_TYPE_CALL 1

KETL_DEFINE(IRUndefinedDelegate) {
	IRUndefinedValue* caller;
	IRUndefinedValue* arguments;
	IRUndefinedValue* next;
	DELEGATE_TYPE type;
};

KETL_DEFINE(BuildState) {
	KETLIRInstruction* first;
	KETLIRInstruction* last;
	KETLObjectPool udelegatePool;
	KETLObjectPool uvaluePool;
	KETLObjectPool valuePool;

	KETLIRValue* stack;
	// TODO namespace
};

static inline IRUndefinedDelegate* wrapInDelegate(BuildState* buildState, KETLIRValue* value) {
	IRUndefinedValue* uvalue = ketlGetFreeObjectFromPool(&buildState->uvaluePool);
	uvalue->value = value;
	uvalue->next = NULL;

	IRUndefinedDelegate* udelegate = ketlGetFreeObjectFromPool(&buildState->udelegatePool);
	udelegate->caller = uvalue;
	udelegate->arguments = NULL;
	udelegate->next = NULL;
	udelegate->type = DELEGATE_TYPE_NONE;

	return udelegate;
}

static inline KETLIRInstruction* createInstruction(BuildState* buildState, KETLObjectPool* irInstructionPool) {
	KETLIRInstruction* instruction = ketlGetFreeObjectFromPool(irInstructionPool);
	if (!buildState->first) {
		buildState->first = buildState->last = instruction;
	}
	else {
		buildState->last = buildState->last->next = instruction;
	}
	return instruction;
}

static inline KETLIRValue* createStackValue(BuildState* buildState) {
	KETLIRValue* stackValue = ketlGetFreeObjectFromPool(&buildState->valuePool);
	stackValue->argType = KETL_INSTRUCTION_ARGUMENT_TYPE_STACK;
	stackValue->argument.stack = 0;
	stackValue->nextOnStack = buildState->stack;
	buildState->stack = stackValue;
	return stackValue;
}

static IRUndefinedDelegate* buildIRFromSyntaxNode(BuildState* buildState, KETLSyntaxNode* syntaxNodeRoot, KETLObjectPool* irInstructionPool) {
	KETLSyntaxNode* it = syntaxNodeRoot;
	switch (it->type) {
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR: {
		KETLSyntaxNode* idNode = it->firstChild;
		if (KETL_CHECK_VOE(idNode->type == KETL_SYNTAX_NODE_TYPE_ID)) {
			return NULL;
		}
			
		KETLIRValue* variable = createStackValue(buildState);
		// TODO deal with namespaces

		// TODO resave saved stack above

		KETLSyntaxNode* expressionNode = idNode->nextSibling;
		IRUndefinedDelegate* expression = buildIRFromSyntaxNode(buildState, expressionNode, irInstructionPool);

		// TODO set type from syntax node or from expression
		variable->type = NULL; // TODO

		KETLIRInstruction* instruction = createInstruction(buildState, irInstructionPool);

		instruction->code = KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES; // TODO choose from type

		instruction->arguments[0] = expression->caller->value; // TODO actual convertion from udelegate to correct type
		instruction->arguments[1] = variable;

		return wrapInDelegate(buildState, variable);
	}
	case KETL_SYNTAX_NODE_TYPE_NUMBER: {
		KETLIRValue* value = ketlGetFreeObjectFromPool(&buildState->valuePool);
		value->type = NULL; // TODO
		value->argType = KETL_INSTRUCTION_ARGUMENT_TYPE_LITERAL;
		value->argument.integer = ketlStrToI32(it->value, it->length);
		value->nextOnStack = NULL;

		return wrapInDelegate(buildState, value);
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS: {
		KETLSyntaxNode* lhsNode = it->firstChild;
		IRUndefinedDelegate* lhs = buildIRFromSyntaxNode(buildState, lhsNode, irInstructionPool);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRUndefinedDelegate* rhs = buildIRFromSyntaxNode(buildState, rhsNode, irInstructionPool);

		KETLIRInstruction* instruction = createInstruction(buildState, irInstructionPool);
			
		switch (it->type) {
		case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS:
			// TODO deduce operation
			instruction->code = KETL_INSTRUCTION_CODE_ADD_INT64;
			break;
		case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS:
			// TODO deduce operation
			instruction->code = KETL_INSTRUCTION_CODE_SUB_INT64;
			break;
		}

		instruction->arguments[0] = lhs->caller->value; // TODO actual convertion from udelegate to correct type
		instruction->arguments[1] = rhs->caller->value; // TODO actual convertion from udelegate to correct type

		KETLIRValue* result = createStackValue(buildState);
		result->type = NULL; // TODO

		return wrapInDelegate(buildState, result);
	}
	default:
		__debugbreak();
	}
	return NULL;
}

static void buildIRSingleCommand(BuildState* buildState, KETLSyntaxNode* syntaxNodeRoot, KETLObjectPool* irInstructionPool) {
	KETLSyntaxNode* it = syntaxNodeRoot;
	while (it) {
		// TODO save current stack
		buildIRFromSyntaxNode(buildState, it, irInstructionPool);
		// restore current stack

		it = it->nextSibling;
	}
}

KETLIRInstruction* ketlBuildIR(KETLSyntaxNode* syntaxNodeRoot, KETLObjectPool* irInstructionPool) {
	BuildState buildState;

	buildState.first = NULL;
	buildState.last = NULL;

	buildState.stack = NULL;

	ketlInitObjectPool(&buildState.valuePool, sizeof(KETLIRValue), 16);
	ketlInitObjectPool(&buildState.uvaluePool, sizeof(IRUndefinedValue), 16);
	ketlInitObjectPool(&buildState.udelegatePool, sizeof(IRUndefinedDelegate), 16);

	buildIRSingleCommand(&buildState, syntaxNodeRoot, irInstructionPool);
	buildState.last->next = NULL;

	ketlDeinitObjectPool(&buildState.udelegatePool);
	ketlDeinitObjectPool(&buildState.uvaluePool);
	ketlDeinitObjectPool(&buildState.valuePool);

	return buildState.first;
}