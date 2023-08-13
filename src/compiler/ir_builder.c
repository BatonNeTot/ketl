//🍲ketl
#include "ir_builder.h"

#include "ir_node.h"
#include "ketl/compiler/syntax_node.h"

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
	stackValue->argTraits.type = KETL_INSTRUCTION_ARGUMENT_TYPE_STACK;
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
	stackValue->argTraits.type = KETL_INSTRUCTION_ARGUMENT_TYPE_STACK;
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
		literal.type = irBuilder->state->primitives.i8_t;
		literal.argument.int8 = (int8_t)intValue;
	}
	else if (INT16_MIN <= intValue && intValue <= INT16_MAX) {
		literal.type = irBuilder->state->primitives.i16_t;
		literal.argument.int16 = (int16_t)intValue;
	}
	else if (INT32_MIN <= intValue && intValue <= INT32_MAX) {
		literal.type = irBuilder->state->primitives.i32_t;
		literal.argument.int32 = (int32_t)intValue;
	}
	else if (INT64_MIN <= intValue && intValue <= INT16_MAX) {
		literal.type = irBuilder->state->primitives.i64_t;
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

static void convertValues(KETLIRBuilder* irBuilder, KETLIRState* irState, CastingOption* castingOption) {
	if (castingOption->value->traits.type == KETL_TRAIT_TYPE_LITERAL) {
		convertLiteralSize(castingOption->value, 
			castingOption->operator ? castingOption->operator->outputType : castingOption->value->type);
	}
	else {
		if (castingOption == NULL || castingOption->operator == NULL) {
			return;
		}

		KETLIRValue* result = createTempVariable(irBuilder, irState);

		KETLCastOperator casting = *castingOption->operator;
		KETLIRInstruction* instruction = createInstruction(irBuilder, irState);
		instruction->code = casting.code;
		instruction->arguments[1] = castingOption->value;

		instruction->arguments[0] = result;
		result->type = casting.outputType;
		result->traits = casting.outputTraits;

		castingOption->value = result;
	}
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
			if (implicit && !it->implicit) {
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

KETL_DEFINE(BinaryOperatorDeduction) {
	KETLBinaryOperator* operator;
	CastingOption* lhsCasting;
	CastingOption* rhsCasting;
};

static BinaryOperatorDeduction deduceInstructionCode2(KETLIRBuilder* irBuilder, KETLSyntaxNodeType syntaxOperator, IRUndefinedDelegate* lhs, IRUndefinedDelegate* rhs) {
	BinaryOperatorDeduction result;
	result.operator = NULL;
	result.lhsCasting = NULL;
	result.rhsCasting = NULL;

	KETLOperatorCode operatorCode = syntaxOperator - KETL_SYNTAX_NODE_TYPE_OPERATOR_OFFSET;
	KETLState* state = irBuilder->state;
	
	KETLBinaryOperator** pOperatorResult = ketlIntMapGet(&state->binaryOperators, operatorCode); 
	if (pOperatorResult == NULL) {
		return result;
	}
	KETLBinaryOperator* operatorResult = *pOperatorResult;

	uint64_t bestScore = UINT64_MAX;

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
					result.operator = it;
					result.lhsCasting = lhsIt;
					result.rhsCasting = rhsIt;
				}
				else if (currentScore == bestScore) {
					result.operator = NULL;
				}
			}
		}
	}

	return result;
}

static CastingOption* castDelegateToVariable(KETLIRBuilder* irBuilder, IRUndefinedDelegate* udelegate, KETLType* type) {
	CastingOption* options = possibleCastingForDelegate(irBuilder, udelegate);
	CastingOption* it = options;

	for (; it; it = it->next) {
		KETLType* castingType = it->operator ? it->operator->outputType : it->value->type;
		if (castingType == type) {
			return it;
		}
	}

	return NULL;
}

static CastingOption* getBestCastingOptionForDelegate(KETLIRBuilder* irBuilder, IRUndefinedDelegate* udelegate) {
	CastingOption* options = possibleCastingForDelegate(irBuilder, udelegate);
	CastingOption* it = options;
	CastingOption* best = NULL;
	uint64_t bestScore = UINT64_MAX;

	for (; it; it = it->next) {
		if (it->score == 0) {
			// TODO temporary fix
			return it;
		}
		if (it->score < bestScore) {
			bestScore = it->score;
			best = it;
		}
		else if (it->score == bestScore) {
			best = NULL;
		}
	}

	return best;
}

static IRUndefinedDelegate* buildIRFromSyntaxNode(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNodeRoot);
static void buildIRBlock(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNode);

static KETLIRValue* createVariableDefinition(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* idNode, KETLType* type) {
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

	if (expression == NULL) {
		// TODO error
		__debugbreak();
	}

	// TODO set traits properly from syntax node
	variable->traits.isConst = false;
	variable->traits.isNullable = false;
	variable->traits.type = KETL_TRAIT_TYPE_LVALUE;

	CastingOption* expressionCasting;

	ketlResetPool(&irBuilder->castingPool);
	if (type == NULL) {
		expressionCasting = getBestCastingOptionForDelegate(irBuilder, expression);
	}
	else {
		expressionCasting = castDelegateToVariable(irBuilder, expression, type);
	}

	if (expressionCasting == NULL) {
		// TODO error
		__debugbreak();
	}

	convertValues(irBuilder, irState, expressionCasting);

	KETLIRValue* expressionValue = expressionCasting->value;
	variable->type = expressionValue->type;

	KETLIRInstruction* instruction = createInstruction(irBuilder, irState);

	instruction->code = KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES; // TODO choose from type

	instruction->arguments[0] = variable;
	instruction->arguments[1] = expressionValue;

	return variable;
}

static KETLIRValue* createJumpLiteral(KETLIRBuilder* irBuilder) {
	KETLIRValue* value = ketlGetFreeObjectFromPool(&irBuilder->valuePool);
	value->argTraits.type = KETL_INSTRUCTION_ARGUMENT_TYPE_LITERAL;
	return value;
}

static IRUndefinedDelegate* buildIRFromSyntaxNode(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNodeRoot) {
	KETLSyntaxNode* it = syntaxNodeRoot;
	switch (it->type) {
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR: {
		KETLSyntaxNode* idNode = it->firstChild;
		KETLIRValue* variable = createVariableDefinition(irBuilder, irState, idNode, NULL);

		if (variable == NULL) {
			return NULL;
		}

		return wrapInDelegateValue(irBuilder, variable);
	}
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR_OF_TYPE: {
		KETLSyntaxNode* typeNode = it->firstChild;

		KETLState* state = irBuilder->state;
		const char* typeName = ketlAtomicStringsGet(&state->strings, typeNode->value, typeNode->length);

		KETLType* type = ketlIntMapGet(&state->globalNamespace.types, (KETLIntMapKey)typeName);

		KETLSyntaxNode* idNode = typeNode->nextSibling;
		KETLIRValue* variable = createVariableDefinition(irBuilder, irState, idNode, type);

		if (variable == NULL) {
			return NULL;
		}

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
		value->argTraits.type = KETL_INSTRUCTION_ARGUMENT_TYPE_LITERAL;
		value->argument = literal.argument;
		value->parent = NULL;
		value->firstChild = NULL;
		value->nextSibling = NULL;

		return wrapInDelegateValue(irBuilder, value);
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PROD:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_DIV:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_EQUAL:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_UNEQUAL: {
		KETLIRValue* result = createTempVariable(irBuilder, irState);

		KETLSyntaxNode* lhsNode = it->firstChild;
		IRUndefinedDelegate* lhs = buildIRFromSyntaxNode(irBuilder, irState, lhsNode);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRUndefinedDelegate* rhs = buildIRFromSyntaxNode(irBuilder, irState, rhsNode);

		ketlResetPool(&irBuilder->castingPool);
		BinaryOperatorDeduction deductionStruct = deduceInstructionCode2(irBuilder, it->type, lhs, rhs);
		if (deductionStruct.operator == NULL) {
			// TODO error
			__debugbreak();
		}

		convertValues(irBuilder, irState, deductionStruct.lhsCasting);
		convertValues(irBuilder, irState, deductionStruct.rhsCasting);

		KETLBinaryOperator deduction = *deductionStruct.operator;
		KETLIRInstruction* instruction = createInstruction(irBuilder, irState);
		instruction->code = deduction.code;
		instruction->arguments[1] = deductionStruct.lhsCasting->value;
		instruction->arguments[2] = deductionStruct.rhsCasting->value;

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
	case KETL_SYNTAX_NODE_TYPE_RETURN: {
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
	KETL_NODEFAULT();
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

static inline void restoreLocalSopeContext(KETLIRState* irState, KETLIRValue* currentStack, KETLIRValue* stackRoot) {
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

static inline void restoreScopeContext(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLIRValue* savedStack, uint64_t scopeIndex) {
	irState->scopeIndex = scopeIndex;
	irState->currentStack = savedStack;

	KETLIntMapIterator iterator;
	ketlInitIntMapIterator(&iterator, &irBuilder->variables);

	while (ketlIntMapIteratorHasNext(&iterator)) {
		const char* name;
		IRUndefinedValue** pCurrent;
		ketlIntMapIteratorGet(&iterator, (KETLIntMapKey*)&name, &pCurrent);
		IRUndefinedValue* current = *pCurrent;
		KETL_FOREVER{
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

static void buildIRBlock(KETLIRBuilder* irBuilder, KETLIRState* irState, KETLSyntaxNode* syntaxNode) {
	for (KETLSyntaxNode* it = syntaxNode; it; it = it->nextSibling) {
		switch (it->type) {
		case KETL_SYNTAX_NODE_TYPE_BLOCK: {

			KETLIRValue* savedStack = irState->currentStack;
			uint64_t scopeIndex = irState->scopeIndex++;

			buildIRBlock(irBuilder, irState, it->firstChild);

			restoreScopeContext(irBuilder, irState, savedStack, scopeIndex);
			break;
		}
		case KETL_SYNTAX_NODE_TYPE_IF_ELSE: {
			KETLIRValue* savedStack = irState->currentStack;
			uint64_t scopeIndex = irState->scopeIndex++;

			KETLIRValue* currentStack = irState->currentStack;
			KETLIRValue* stackRoot = irState->stackRoot;

			irState->tempVariables = NULL;
			irState->localVariables = NULL;

			KETLSyntaxNode* expressionNode = it->firstChild;
			IRUndefinedDelegate* expression = buildIRFromSyntaxNode(irBuilder, irState, expressionNode);

			CastingOption* expressionCasting = castDelegateToVariable(irBuilder, expression, irBuilder->state->primitives.bool_t);
			if (expressionCasting == NULL) {
				// TODO error
				__debugbreak();
			}
			convertValues(irBuilder, irState, expressionCasting);

			restoreLocalSopeContext(irState, currentStack, stackRoot);

			KETLIRInstruction* ifJumpInstruction = createInstruction(irBuilder, irState);
			ifJumpInstruction->code = KETL_INSTRUCTION_CODE_JUMP_IF_FALSE;
			ifJumpInstruction->arguments[0] = createJumpLiteral(irBuilder);
			ifJumpInstruction->arguments[1] = expressionCasting->value;

			KETLSyntaxNode* trueBlockNode = expressionNode->nextSibling;
			// builds all instructions
			buildIRBlock(irBuilder, irState, trueBlockNode);

			if (trueBlockNode->nextSibling != NULL) {
				KETLIRInstruction* jumpInstruction = createInstruction(irBuilder, irState);
				jumpInstruction->code = KETL_INSTRUCTION_CODE_JUMP;
				jumpInstruction->arguments[0] = createJumpLiteral(irBuilder);

				KETLIRInstruction* labelAfterTrueBlockInstruction = createInstruction(irBuilder, irState);
				labelAfterTrueBlockInstruction->code = KETL_INSTRUCTION_CODE_NONE;
				ifJumpInstruction->arguments[0]->argument.globalPtr = labelAfterTrueBlockInstruction;

				KETLSyntaxNode* falseBlockNode = trueBlockNode->nextSibling;
				// builds all instructions
				buildIRBlock(irBuilder, irState, falseBlockNode);

				KETLIRInstruction* labelAfterFalseBlockInstruction = createInstruction(irBuilder, irState);
				labelAfterFalseBlockInstruction->code = KETL_INSTRUCTION_CODE_NONE;
				jumpInstruction->arguments[0]->argument.globalPtr = labelAfterFalseBlockInstruction;
			}
			else {
				KETLIRInstruction* labelAfterTrueBlockInstruction = createInstruction(irBuilder, irState);
				labelAfterTrueBlockInstruction->code = KETL_INSTRUCTION_CODE_NONE;
				ifJumpInstruction->arguments[0]->argument.globalPtr = labelAfterTrueBlockInstruction;

			}

			restoreScopeContext(irBuilder, irState, savedStack, scopeIndex);
			break;
		}
		default: {
			KETLIRValue* currentStack = irState->currentStack;
			KETLIRValue* stackRoot = irState->stackRoot;

			irState->tempVariables = NULL;
			irState->localVariables = NULL;

			buildIRFromSyntaxNode(irBuilder, irState, it);

			restoreLocalSopeContext(irState, currentStack, stackRoot);
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