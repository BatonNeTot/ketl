//🍲ketl
#include "ir_builder.h"

#include "syntax_node.h"
#include "ir_node.h"

#include "ketl/object_pool.h"
#include "ketl/atomic_strings.h"
#include "ketl/type.h"
#include "ketl/ketl.h"
#include "ketl/assert.h"

KETL_DEFINE(IRUndefinedValue) {
	KETLIRValue* value;
	uint64_t scopeIndex;
	IRUndefinedValue* next;
};

KETL_DEFINE(IRUndefinedDelegate) {
	IRUndefinedValue* caller;
	IRUndefinedValue* arguments;
	IRUndefinedValue* next;
};

KETL_DEFINE(CastingOption) {
	KETLCastOperator* operator;
	KETLIRValue* value;
	uint64_t score;
	CastingOption* next;
};

void ketlInitIRBuilder(KETLIRBuilder* irBuilder, KETLState* state) {
	ketlInitIntMap(&irBuilder->variables, sizeof(IRUndefinedValue*), 16);
	ketlInitObjectPool(&irBuilder->valuePool, sizeof(KETLIRValue), 16);
	ketlInitObjectPool(&irBuilder->uvaluePool, sizeof(IRUndefinedValue), 16);
	ketlInitObjectPool(&irBuilder->udelegatePool, sizeof(IRUndefinedDelegate), 16);
	ketlInitObjectPool(&irBuilder->irInstructionPool, sizeof(KETLIRInstruction), 16);
	ketlInitObjectPool(&irBuilder->castingPool, sizeof(CastingOption), 16);
	irBuilder->state = state;
}

void ketlDeinitIRBuilder(KETLIRBuilder* irBuilder) {
	ketlDeinitObjectPool(&irBuilder->castingPool);
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

KETL_DEFINE(Literal) {
	KETLType* type;
	KETLInstructionArgument argument;
};

static inline Literal parseLiteral(KETLIRBuilder* irBuilder, const char* value, size_t length) {
	Literal literal;
	int64_t intValue = ketlStrToI64(value, length);
	if (INT8_MIN <= intValue && intValue <= INT8_MAX) {
		literal.type = irBuilder->state->primitives[0];
		literal.argument.int8 = (int8_t)intValue;
	}
	else if (INT16_MIN <= intValue && intValue <= INT16_MAX) {
		literal.type = irBuilder->state->primitives[1];
		literal.argument.int16 = (int16_t)intValue;
	}
	else if (INT32_MIN <= intValue && intValue <= INT32_MAX) {
		literal.type = irBuilder->state->primitives[2];
		literal.argument.int32 = (int32_t)intValue;
	}
	else if (INT64_MIN <= intValue && intValue <= INT16_MAX) {
		literal.type = irBuilder->state->primitives[3];
		literal.argument.int64 = intValue;
	}
	return literal;
}

static inline convertLiteralSize(KETLIRValue* value, KETLType* targetType) {
	switch (value->type->size) {
	case 1:
		switch (targetType->size) {
		case 2: value->argument.uint16 = value->argument.uint8;
			break;
		case 4: value->argument.uint32 = value->argument.uint8;
			break;
		case 8: value->argument.uint64 = value->argument.uint8;
			break;
		}
		break;
	case 2:
		switch (targetType->size) {
			// TODO warning
		case 1: value->argument.uint8 = (uint8_t)value->argument.uint16;
			break;
		case 4: value->argument.uint32 = value->argument.uint16;
			break;
		case 8: value->argument.uint64 = value->argument.uint16;
			break;
		}
		break;
	case 4:
		switch (targetType->size) {
			// TODO warning
		case 1: value->argument.uint8 = (uint8_t)value->argument.uint32;
			break;
			// TODO warning
		case 2: value->argument.uint16 = (uint16_t)value->argument.uint32;
			break;
		case 8: value->argument.uint64 = value->argument.uint32;
			break;
		}
		break;
	case 8:
		switch (targetType->size) {
			// TODO warning
		case 1: value->argument.uint8 = (uint8_t)value->argument.uint64;
			break;
			// TODO warning
		case 2: value->argument.uint16 = (uint16_t)value->argument.uint64;
			break;
			// TODO warning
		case 4: value->argument.uint32 = (uint32_t)value->argument.uint64;
			break;
		}
		break;
	}
	value->type = targetType;
}

KETL_DEFINE(TypeCastingTargetList) {
	CastingOption* begin;
	CastingOption* last;
};

static TypeCastingTargetList possibleCastingForValue(KETLIRBuilder* irBuilder, KETLIRValue* value, bool implicit) {
	TypeCastingTargetList targets;
	targets.begin = targets.last = NULL;

	bool numberLiteral = value->traits.type == KETL_TRAIT_TYPE_LITERAL && value->type->kind == KETL_TYPE_KIND_PRIMITIVE;
#define MAX_SIZE_OF_NUMBER_LITERAL 8

	KETLCastOperator** castOperators = ketlIntMapGet(&irBuilder->state->castOperators, (KETLIntMapKey)value->type);
	if (castOperators != NULL) {
		KETLCastOperator* it = *castOperators;
		for (; it; it = it->next) {
			if (!implicit && it->implicit) {
				continue;
			}

			// TODO check traits

			CastingOption* newTarget = ketlGetFreeObjectFromPool(&irBuilder->castingPool);
			newTarget->value = value;
			newTarget->operator = it;

			if (targets.last == NULL) {
				targets.last = newTarget;
			}
			newTarget->next = targets.begin;
			targets.begin = newTarget;

			newTarget->score = !numberLiteral || it->outputType->size != MAX_SIZE_OF_NUMBER_LITERAL;
		}
	}

	CastingOption* newTarget = ketlGetFreeObjectFromPool(&irBuilder->castingPool);
	newTarget->value = value;
	newTarget->operator = NULL;

	if (targets.last == NULL) {
		targets.last = newTarget;
	}
	newTarget->next = targets.begin;
	targets.begin = newTarget;

	newTarget->score = numberLiteral && value->type->size != MAX_SIZE_OF_NUMBER_LITERAL;

	return targets;
}

static CastingOption* possibleCastingForDelegate(KETLIRBuilder* irBuilder, IRUndefinedDelegate* udelegate) {
	CastingOption* targets = NULL;

	IRUndefinedValue* callerIt = udelegate->caller;
	for (; callerIt; callerIt = callerIt->next) {
		KETLIRValue* callerValue = callerIt->value;
		KETLType* callerType = callerValue->type;


		KETLIRValue* outputValue = callerValue;

		if (callerType->kind == KETL_TYPE_KIND_FUNCTION) {
			__debugbreak();
		}
		else {
			if (udelegate->arguments) {
				// only functions can use arguments
				continue;
			}
		}

		TypeCastingTargetList castingTargets = possibleCastingForValue(irBuilder, outputValue, true);
		if (castingTargets.begin != NULL) {
			castingTargets.last->next = targets;
			targets = castingTargets.begin;
		}
	}

	return targets;
}

static KETLBinaryOperator* deduceInstructionCode2(KETLIRBuilder* irBuilder, KETLSyntaxNodeType syntaxOperator, IRUndefinedDelegate* lhs, IRUndefinedDelegate* rhs) {
	KETLOperatorCode operatorCode = syntaxOperator - KETL_SYNTAX_NODE_TYPE_OPERATOR_OFFSET;
	KETLState* state = irBuilder->state;
	
	KETLBinaryOperator** pOperatorResult = ketlIntMapGet(&state->binaryOperators, operatorCode); 
	if (pOperatorResult == NULL) {
		return NULL;
	}
	KETLBinaryOperator* operatorResult = *pOperatorResult;

	ketlResetPool(&irBuilder->castingPool);

	uint64_t bestScore = UINT64_MAX;
	KETLBinaryOperator* bestScoreOperator = NULL;

	CastingOption* bestLhsCasting = NULL;
	CastingOption* bestRhsCasting = NULL;

	uint64_t currentScore;

	CastingOption* lhsCasting = possibleCastingForDelegate(irBuilder, lhs);
	CastingOption* rhsCasting = possibleCastingForDelegate(irBuilder, rhs);

	KETLBinaryOperator* it = operatorResult;
	for (; it; it = it->next) {
		currentScore = 0;
		CastingOption* lhsIt = lhsCasting;
		for (; lhsIt; lhsIt = lhsIt->next) {
			KETLType* lhsType = lhsIt->operator ? lhsIt->operator->outputType : lhsIt->value->type;
			if (it->lhsType != lhsType) {
				continue;
			}

			CastingOption* rhsIt = rhsCasting;
			for (; rhsIt; rhsIt = rhsIt->next) {
				KETLType* rhsType = rhsIt->operator ? rhsIt->operator->outputType : rhsIt->value->type;
				if (it->rhsType != rhsType) {
					continue;
				}

				currentScore += lhsIt->score;
				currentScore += rhsIt->score;

				if (currentScore < bestScore) {
					bestScore = currentScore;
					bestScoreOperator = it;
					bestLhsCasting = lhsIt;
					bestRhsCasting = rhsIt;
				}
				else if (currentScore == bestScore) {
					bestScoreOperator = NULL;
				}
			}
		}
	}

	if (bestScoreOperator == NULL) {
		return NULL;
	}

	convertLiteralSize(bestLhsCasting->value, bestLhsCasting->operator ? bestLhsCasting->operator->outputType : bestLhsCasting->value->type);
	convertLiteralSize(bestRhsCasting->value, bestRhsCasting->operator ? bestRhsCasting->operator->outputType : bestRhsCasting->value->type);

	return bestScoreOperator;
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

		KETLSyntaxNode* expressionNode = idNode->nextSibling;
		IRUndefinedDelegate* expression = buildIRFromSyntaxNode(irBuilder, irState, expressionNode);

		const char* name = ketlAtomicStringsGet(&irBuilder->state->strings, idNode->value, idNode->length);
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

		// TODO set type from syntax node if exist
		variable->type = expression->caller->value->type;
		variable->traits = expression->caller->value->traits;

		KETLIRInstruction* instruction = createInstruction(irBuilder, irState);

		instruction->code = KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES; // TODO choose from type

		instruction->arguments[0] = variable;
		instruction->arguments[1] = expression->caller->value; // TODO actual convertion from udelegate to correct type

		return wrapInDelegateValue(irBuilder, variable);
	}
	case KETL_SYNTAX_NODE_TYPE_ID: {
		const char* uniqName = ketlAtomicStringsGet(&irBuilder->state->strings, it->value, it->length);
		IRUndefinedValue** ppValue;
		if (ketlIntMapGetOrCreate(&irBuilder->variables, (KETLIntMapKey)uniqName, &ppValue)) {
			// TODO error
			__debugbreak();
		}
		return wrapInDelegateUValue(irBuilder, *ppValue);
	}
	case KETL_SYNTAX_NODE_TYPE_NUMBER: {
		Literal literal = parseLiteral(irBuilder, it->value, it->length);
		if (literal.type == NULL) {
			// TODO error
		}
		KETLIRValue* value = ketlGetFreeObjectFromPool(&irBuilder->valuePool);
		value->name = NULL;
		value->type = literal.type;
		value->traits.isNullable = false;
		value->traits.isConst = true;
		value->traits.type = KETL_TRAIT_TYPE_LITERAL;
		value->argType = KETL_INSTRUCTION_ARGUMENT_TYPE_LITERAL;
		value->argument = literal.argument;
		value->parent = NULL;
		value->firstChild = NULL;
		value->nextSibling = NULL;

		return wrapInDelegateValue(irBuilder, value);
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PROD:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_DIV: {
		KETLIRValue* result = createTempVariable(irBuilder, irState);

		KETLSyntaxNode* lhsNode = it->firstChild;
		IRUndefinedDelegate* lhs = buildIRFromSyntaxNode(irBuilder, irState, lhsNode);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRUndefinedDelegate* rhs = buildIRFromSyntaxNode(irBuilder, irState, rhsNode);
			
		KETLBinaryOperator* pDeduction = deduceInstructionCode2(irBuilder, it->type, lhs, rhs);
		if (pDeduction == NULL) {
			// TODO error
			__debugbreak();
		}

		KETLBinaryOperator deduction = *pDeduction;
		KETLIRInstruction* instruction = createInstruction(irBuilder, irState);
		instruction->code = deduction.code;
		instruction->arguments[1] = lhs->caller->value; // TODO actual convertion from udelegate to correct type
		instruction->arguments[2] = rhs->caller->value; // TODO actual convertion from udelegate to correct type

		instruction->arguments[0] = result;
		result->type = deduction.outputType;
		result->traits = deduction.outputTraits;

		return wrapInDelegateValue(irBuilder, result);
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

		return wrapInDelegateValue(irBuilder, instruction->arguments[0]);
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
			KETLIRValue* savedStack = irState->currentStack;
			uint64_t scopeIndex = irState->scopeIndex++;

			buildIRBlock(irBuilder, irState, it->firstChild);

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

void ketlBuildIR(KETLType* returnType, KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNodeRoot) {
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

	buildIRBlock(irBuilder, irState, syntaxNodeRoot);
	if (irState->last) {
		irState->last->next = NULL;
	}

	// TODO check and cast return states
}