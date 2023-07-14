//🍲ketl
#include "ir_builder.h"

#include "syntax_node.h"
#include "ir_node.h"

#include "ketl/object_pool.h"
#include "ketl/assert.h"

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

void ketlInitIRBuilder(KETLIRBuilder* irBuilder) {
	ketlInitObjectPool(&irBuilder->valuePool, sizeof(KETLIRValue), 16);
	ketlInitObjectPool(&irBuilder->uvaluePool, sizeof(IRUndefinedValue), 16);
	ketlInitObjectPool(&irBuilder->udelegatePool, sizeof(IRUndefinedDelegate), 16);
	ketlInitObjectPool(&irBuilder->irInstructionPool, sizeof(KETLIRInstruction), 16);
}

void ketlDeinitIRBuilder(KETLIRBuilder* irBuilder) {
	ketlDeinitObjectPool(&irBuilder->irInstructionPool);
	ketlDeinitObjectPool(&irBuilder->udelegatePool);
	ketlDeinitObjectPool(&irBuilder->uvaluePool);
	ketlDeinitObjectPool(&irBuilder->valuePool);
}

static inline IRUndefinedDelegate* wrapInDelegate(KETLIRBuilder* irBuilder, KETLIRValue* value) {
	IRUndefinedValue* uvalue = ketlGetFreeObjectFromPool(&irBuilder->uvaluePool);
	uvalue->value = value;
	uvalue->next = NULL;

	IRUndefinedDelegate* udelegate = ketlGetFreeObjectFromPool(&irBuilder->udelegatePool);
	udelegate->caller = uvalue;
	udelegate->arguments = NULL;
	udelegate->next = NULL;
	udelegate->type = DELEGATE_TYPE_NONE;

	return udelegate;
}

static inline KETLIRInstruction* createInstruction(KETLIRBuilder* irBuilder, KETLIRState* irState) {
	KETLIRInstruction* instruction = ketlGetFreeObjectFromPool(&irBuilder->irInstructionPool);
	if (!irState->first) {
		irState->first = irState->last = instruction;
	}
	else {
		irState->last = irState->last->next = instruction;
	}
	return instruction;
}

static inline KETLIRValue* createTempVariable(KETLIRBuilder* irBuilder, KETLIRState* irState) {
	KETLIRValue* stackValue = ketlGetFreeObjectFromPool(&irBuilder->valuePool);
	stackValue->argType = KETL_INSTRUCTION_ARGUMENT_TYPE_STACK;
	stackValue->argument.stack = 0;
	stackValue->firstChild = NULL;
	stackValue->nextSibling = NULL;

	KETLIRValue* tempVariables = irState->tempVariables;
	if (tempVariables != NULL) {
		stackValue->parent = tempVariables;
		tempVariables->firstChild = stackValue;
		irState->tempVariables = stackValue;
		return stackValue;
	}
	else {
		stackValue->parent = NULL;
		irState->tempVariables = stackValue;
		return stackValue;
	}
}

static inline KETLIRValue* createLocalVariable(KETLIRBuilder* irBuilder, KETLIRState* irState) {
	KETLIRValue* stackValue = ketlGetFreeObjectFromPool(&irBuilder->valuePool);
	stackValue->argType = KETL_INSTRUCTION_ARGUMENT_TYPE_STACK;
	stackValue->argument.stack = 0;
	stackValue->firstChild = NULL;
	stackValue->nextSibling = NULL;

	KETLIRValue* localVariables = irState->localVariables;
	if (localVariables != NULL) {
		stackValue->parent = localVariables;
		localVariables->firstChild = stackValue;
		irState->localVariables = stackValue;
		return stackValue;
	}
	else {
		stackValue->parent = NULL;
		irState->localVariables = stackValue;
		return stackValue;
	}
}
static IRUndefinedDelegate* buildIRFromSyntaxNode(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNodeRoot) {
	KETLSyntaxNode* it = syntaxNodeRoot;
	switch (it->type) {
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR: {
		KETLSyntaxNode* idNode = it->firstChild;
		if (KETL_CHECK_VOE(idNode->type == KETL_SYNTAX_NODE_TYPE_ID)) {
			return NULL;
		}
			
		KETLIRValue* variable = createLocalVariable(irBuilder, irState);
		// TODO deal with namespaces

		KETLSyntaxNode* expressionNode = idNode->nextSibling;
		IRUndefinedDelegate* expression = buildIRFromSyntaxNode(irBuilder, irState, expressionNode);

		// TODO set type from syntax node or from expression
		variable->type = NULL; // TODO

		KETLIRInstruction* instruction = createInstruction(irBuilder, irState);

		instruction->code = KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES; // TODO choose from type

		instruction->arguments[0] = variable;
		instruction->arguments[1] = expression->caller->value; // TODO actual convertion from udelegate to correct type

		return wrapInDelegate(irBuilder, variable);
	}
	case KETL_SYNTAX_NODE_TYPE_ID: {
		KETLIRValue* value = ketlGetFreeObjectFromPool(&irBuilder->valuePool);

		// TODO deal with namespaces

		return wrapInDelegate(irBuilder, value);
	}
	case KETL_SYNTAX_NODE_TYPE_NUMBER: {
		KETLIRValue* value = ketlGetFreeObjectFromPool(&irBuilder->valuePool);
		value->type = NULL; // TODO
		value->argType = KETL_INSTRUCTION_ARGUMENT_TYPE_LITERAL;
		value->argument.integer = ketlStrToI32(it->value, it->length);
		value->parent = NULL;
		value->firstChild = NULL;
		value->nextSibling = NULL;

		return wrapInDelegate(irBuilder, value);
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS: {
		KETLIRValue* result = createTempVariable(irBuilder, irState);
		result->type = NULL; // TODO

		KETLSyntaxNode* lhsNode = it->firstChild;
		IRUndefinedDelegate* lhs = buildIRFromSyntaxNode(irBuilder, irState, lhsNode);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRUndefinedDelegate* rhs = buildIRFromSyntaxNode(irBuilder, irState, rhsNode);

		KETLIRInstruction* instruction = createInstruction(irBuilder, irState);
			
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

		instruction->arguments[1] = lhs->caller->value; // TODO actual convertion from udelegate to correct type
		instruction->arguments[2] = rhs->caller->value; // TODO actual convertion from udelegate to correct type

		instruction->arguments[0] = result;

		return wrapInDelegate(irBuilder, result);
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_ASSIGN: {
		KETLSyntaxNode* lhsNode = it->firstChild;
		IRUndefinedDelegate* lhs = buildIRFromSyntaxNode(irBuilder, irState, lhsNode);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRUndefinedDelegate* rhs = buildIRFromSyntaxNode(irBuilder, irState, rhsNode);

		KETLIRInstruction* instruction = createInstruction(irBuilder, irState);

		instruction->code = KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES; // TODO choose from type

		instruction->arguments[0] = lhs->caller->value; // TODO actual convertion from udelegate to correct type
		instruction->arguments[1] = rhs->caller->value; // TODO actual convertion from udelegate to correct type

		return wrapInDelegate(irBuilder, instruction->arguments[0]);
	}
	case  KETL_SYNTAX_NODE_TYPE_RETURN: {

		KETLSyntaxNode* expressionNode = it->firstChild;
		if (expressionNode) {
			IRUndefinedDelegate* expression = buildIRFromSyntaxNode(irBuilder, irState, expressionNode);

			KETLIRInstruction* instruction = createInstruction(irBuilder, irState);
			instruction->code = KETL_INSTRUCTION_CODE_RETURN_8_BYTES; // TODO deside from type
			instruction->arguments[0] = expression->caller->value; // TODO actual convertion from udelegate to correct type
		}
		else {
			KETLIRInstruction* instruction = createInstruction(irBuilder, irState);
			instruction->code = KETL_INSTRUCTION_CODE_RETURN;
		}

		return NULL;
	}
	default:
		__debugbreak();
	}
	return NULL;
}

#define UPDATE_NODE_TALE(firstVariable)\
do {\
	while (firstVariable->parent) {\
	firstVariable = firstVariable->parent;\
	}\
	\
	if (currentStack != NULL) {\
		KETLIRValue* lastChild = currentStack->firstChild;\
		if (lastChild != NULL) {\
			while (lastChild->nextSibling) {\
				lastChild = lastChild->nextSibling;\
			}\
			lastChild->nextSibling = firstVariable;\
		}\
		else {\
			currentStack->firstChild = firstVariable;\
		}\
		firstVariable->parent = currentStack;\
	}\
	else {\
		if (stackRoot != NULL) {\
			KETLIRValue* lastChild = stackRoot;\
			while (lastChild->nextSibling) {\
				lastChild = lastChild->nextSibling;\
			}\
			lastChild->nextSibling = firstVariable;\
		}\
		else {\
			stackRoot = firstVariable;\
		}\
		firstVariable->parent = NULL;\
	}\
} while(0)\

static void buildIRSingleCommand(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNode) {
	KETLIRValue* currentStack = irState->currentStack;
	KETLIRValue* stackRoot = irState->stackRoot;

	irState->tempVariables = NULL;
	irState->localVariables = NULL;

	buildIRFromSyntaxNode(irBuilder, irState, syntaxNode);

	{
		// set local variable
		KETLIRValue* firstLocalVariable = irState->localVariables;
		if (firstLocalVariable != NULL) {
			KETLIRValue* lastLocalVariables = firstLocalVariable;
			UPDATE_NODE_TALE(firstLocalVariable);
			currentStack = firstLocalVariable;
		}
	}

	{
		// set temp variable
		KETLIRValue* firstTempVariable = irState->tempVariables;
		if (firstTempVariable) {
			UPDATE_NODE_TALE(firstTempVariable);
		}
	}

	irState->currentStack = currentStack;
	irState->stackRoot = stackRoot;
}

static void buildIRBlock(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNode) {
	for (KETLSyntaxNode* it = syntaxNode; it; it = it->nextSibling) {
		if (it->type != KETL_SYNTAX_NODE_TYPE_BLOCK) {
			buildIRSingleCommand(irBuilder, irState, it);
		}
		else {
			// TODO prepare scope
			KETLIRValue* savedStack = irState->currentStack;
			buildIRBlock(irBuilder, irState, it->firstChild);
			// TODO clear scope
			irState->currentStack = savedStack;
		}
	}
}

void ketlBuildIR(KETLType* returnType, KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNodeRoot) {
	ketlResetPool(&irBuilder->irInstructionPool);
	ketlResetPool(&irBuilder->udelegatePool);
	ketlResetPool(&irBuilder->uvaluePool);
	ketlResetPool(&irBuilder->valuePool);

	irState->first = NULL;
	irState->last = NULL;

	irState->stackRoot = NULL;
	irState->currentStack = NULL;

	buildIRBlock(irBuilder, irState, syntaxNodeRoot);
	if (irState->last) {
		irState->last->next = NULL;
	}

	// TODO check and cast return states
}