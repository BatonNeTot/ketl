//🍲ketl
#include "ir_builder.h"

#include "syntax_node.h"
#include "ir_node.h"

#include "ketl/object_pool.h"
#include "ketl/atomic_strings.h"
#include "ketl/assert.h"

KETL_DEFINE(IRUndefinedValue) {
	KETLIRValue* value;
	uint64_t scopeIndex;
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
	ketlInitIntMap(&irBuilder->variables, sizeof(IRUndefinedValue*), 16);
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
	ketlDeinitIntMap(&irBuilder->variables);
}

static inline IRUndefinedDelegate* wrapInDelegateUValue(KETLIRBuilder* irBuilder, IRUndefinedValue* uvalue) {
	IRUndefinedDelegate* udelegate = ketlGetFreeObjectFromPool(&irBuilder->udelegatePool);
	udelegate->caller = uvalue;
	udelegate->arguments = NULL;
	udelegate->next = NULL;
	udelegate->type = DELEGATE_TYPE_NONE;

	return udelegate;
}

static inline IRUndefinedValue* wrapInUValueValue(KETLIRBuilder* irBuilder, KETLIRValue* value) {
	IRUndefinedValue* uvalue = ketlGetFreeObjectFromPool(&irBuilder->uvaluePool);
	uvalue->value = value;
	uvalue->next = NULL;

	return uvalue;
}

static inline IRUndefinedDelegate* wrapInDelegateValue(KETLIRBuilder* irBuilder, KETLIRValue* value) {
	return wrapInDelegateUValue(irBuilder, wrapInUValueValue(irBuilder, value));
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
	stackValue->name = NULL;
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
static IRUndefinedDelegate* buildIRFromSyntaxNode(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNodeRoot, KETLAtomicStrings* strings) {
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
		IRUndefinedDelegate* expression = buildIRFromSyntaxNode(irBuilder, irState, expressionNode, strings);

		const char* name = ketlAtomicStringsGet(strings, idNode->value, idNode->length);
		variable->name = name;

		uint64_t scopeIndex = irState->scopeIndex;

		IRUndefinedValue** pCurrent;
		if (ketlIntMapGetOrCreate(&irBuilder->variables, (KETLIntMapKey)name, &pCurrent)) {
			*pCurrent = NULL;
		}
		else {
			// TODO check if the type is function, then we can overload
			if ((*pCurrent)->scopeIndex == scopeIndex) {
				// TODO error
				__debugbreak();
			}
		}
		IRUndefinedValue* uvalue = wrapInUValueValue(irBuilder, variable);
		uvalue->scopeIndex = scopeIndex;
		uvalue->next = *pCurrent;
		*pCurrent = uvalue;

		// TODO set type from syntax node or from expression
		variable->type = NULL; // TODO

		KETLIRInstruction* instruction = createInstruction(irBuilder, irState);

		instruction->code = KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES; // TODO choose from type

		instruction->arguments[0] = variable;
		instruction->arguments[1] = expression->caller->value; // TODO actual convertion from udelegate to correct type

		return wrapInDelegateValue(irBuilder, variable);
	}
	case KETL_SYNTAX_NODE_TYPE_ID: {
		const char* uniqName = ketlAtomicStringsGet(strings, it->value, it->length);
		IRUndefinedValue** ppValue;
		if (ketlIntMapGetOrCreate(&irBuilder->variables, (KETLIntMapKey)uniqName, &ppValue)) {
			// TODO error
			__debugbreak();
		}
		return wrapInDelegateUValue(irBuilder, *ppValue);
	}
	case KETL_SYNTAX_NODE_TYPE_NUMBER: {
		KETLIRValue* value = ketlGetFreeObjectFromPool(&irBuilder->valuePool);
		value->name = NULL;
		value->type = NULL; // TODO
		value->argType = KETL_INSTRUCTION_ARGUMENT_TYPE_LITERAL;
		value->argument.integer = ketlStrToI32(it->value, it->length);
		value->parent = NULL;
		value->firstChild = NULL;
		value->nextSibling = NULL;

		return wrapInDelegateValue(irBuilder, value);
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS: {
		KETLIRValue* result = createTempVariable(irBuilder, irState);
		result->type = NULL; // TODO

		KETLSyntaxNode* lhsNode = it->firstChild;
		IRUndefinedDelegate* lhs = buildIRFromSyntaxNode(irBuilder, irState, lhsNode, strings);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRUndefinedDelegate* rhs = buildIRFromSyntaxNode(irBuilder, irState, rhsNode, strings);

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

		return wrapInDelegateValue(irBuilder, result);
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_ASSIGN: {
		KETLSyntaxNode* lhsNode = it->firstChild;
		IRUndefinedDelegate* lhs = buildIRFromSyntaxNode(irBuilder, irState, lhsNode, strings);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRUndefinedDelegate* rhs = buildIRFromSyntaxNode(irBuilder, irState, rhsNode, strings);

		KETLIRInstruction* instruction = createInstruction(irBuilder, irState);

		instruction->code = KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES; // TODO choose from type

		instruction->arguments[0] = lhs->caller->value; // TODO actual convertion from udelegate to correct type
		instruction->arguments[1] = rhs->caller->value; // TODO actual convertion from udelegate to correct type

		return wrapInDelegateValue(irBuilder, instruction->arguments[0]);
	}
	case  KETL_SYNTAX_NODE_TYPE_RETURN: {

		KETLSyntaxNode* expressionNode = it->firstChild;
		if (expressionNode) {
			IRUndefinedDelegate* expression = buildIRFromSyntaxNode(irBuilder, irState, expressionNode, strings);

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

static void buildIRSingleCommand(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNode, KETLAtomicStrings* strings) {
	KETLIRValue* currentStack = irState->currentStack;
	KETLIRValue* stackRoot = irState->stackRoot;

	irState->tempVariables = NULL;
	irState->localVariables = NULL;

	buildIRFromSyntaxNode(irBuilder, irState, syntaxNode, strings);

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

static void buildIRBlock(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNode, KETLAtomicStrings* strings) {
	for (KETLSyntaxNode* it = syntaxNode; it; it = it->nextSibling) {
		if (it->type != KETL_SYNTAX_NODE_TYPE_BLOCK) {
			buildIRSingleCommand(irBuilder, irState, it, strings);
		}
		else {
			KETLIRValue* savedStack = irState->currentStack;
			uint64_t scopeIndex = irState->scopeIndex++;

			buildIRBlock(irBuilder, irState, it->firstChild, strings);

			irState->scopeIndex = scopeIndex;
			irState->currentStack = savedStack;

			KETLIntMapIterator iterator;
			ketlInitIntMapIterator(&iterator, &irBuilder->variables);

			while (ketlIntMapIteratorHasNext(&iterator)) {
				const char* name;
				IRUndefinedValue** pCurrent;
				ketlIntMapIteratorGet(&iterator, (KETLIntMapKey*)&name, &pCurrent);
				IRUndefinedValue* current = *pCurrent;
				KETL_FOREVER {
					if (current == NULL) {
						ketlIntMapIteratorRemove(&iterator);
						break;
					}

					if (current->scopeIndex <= scopeIndex) {
						*pCurrent = current;
						ketlIntMapIteratorNext(&iterator);
						break;
					}

					current = current->next;
				}
			}
		}
	}
}

void ketlBuildIR(KETLType* returnType, KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNodeRoot, KETLAtomicStrings* strings) {
	ketlIntMapReset(&irBuilder->variables);
	ketlResetPool(&irBuilder->irInstructionPool);
	ketlResetPool(&irBuilder->udelegatePool);
	ketlResetPool(&irBuilder->uvaluePool);
	ketlResetPool(&irBuilder->valuePool);

	irState->first = NULL;
	irState->last = NULL;

	irState->stackRoot = NULL;
	irState->currentStack = NULL;

	irState->scopeIndex = 0;

	buildIRBlock(irBuilder, irState, syntaxNodeRoot, strings);
	if (irState->last) {
		irState->last->next = NULL;
	}

	// TODO check and cast return states
}