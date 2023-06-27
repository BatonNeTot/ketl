//🍲ketl
#include "ketl/function.h"

static uint8_t CODE_SIZES[] = {
		1,	//None,
		4,	//AddInt64,
		4,	//MinusInt64,
		4,	//MultyInt64,
		4,	//DivideInt64,
		3,	//Assign8bytes,
		1,	//Return,
};

#define CODE_SIZE(x) CODE_SIZES[x]

inline static void* getArgument(uint8_t* stackPtr, KETLInstructionArgumentType type, KETLInstructionArgument* value) {
	switch (type) {
	case KETL_INSTRUCTION_ARGUMENT_TYPE_GLOBAL: {
		return value->globalPtr;
	}
	case KETL_INSTRUCTION_ARGUMENT_TYPE_STACK: {
		return stackPtr + value->stack;
	}
	case KETL_INSTRUCTION_ARGUMENT_TYPE_LITERAL: {
		return value;
	}
	case KETL_INSTRUCTION_ARGUMENT_TYPE_FUNCTION_PARAMETER: {
		return *(uint8_t**)(stackPtr + value->stack);
	}
	}
	return NULL;
}

#define ARGUMENT(number, type)\
(*(type*)(getArgument(stackPtr, instruction.argumentTypes[(number) + 1], &(pInstruction + (number) + 1)->argument)))


void ketlCallFunction(KETLFunction* function, void* _stackPtr, void* returnPtr) {
	uint64_t* pIndex = (uint64_t*)(_stackPtr);
	uint64_t index = *pIndex = 0;
	uint8_t* stackPtr = ((uint8_t*)(_stackPtr)) + sizeof(index);
	KETL_FOREVER {
		KETLInstruction* pInstruction = ((KETLInstruction*)(function + 1)) + index;
		KETLInstruction instruction = *pInstruction;
		switch (instruction.code) {
		case KETL_INSTRUCTION_CODE_ADD_INT64: {
			ARGUMENT(2, int64_t) = ARGUMENT(0, int64_t) + ARGUMENT(1, int64_t);
			break;
		}
		case KETL_INSTRUCTION_CODE_SUB_INT64: {
			ARGUMENT(2, int64_t) = ARGUMENT(0, int64_t) - ARGUMENT(1, int64_t);
			break;
		}
		case KETL_INSTRUCTION_CODE_MULTY_INT64: {
			ARGUMENT(2, int64_t) = ARGUMENT(0, int64_t) * ARGUMENT(1, int64_t);
			break;
		}
		case KETL_INSTRUCTION_CODE_DIV_INT64: {
			ARGUMENT(2, int64_t) = ARGUMENT(0, int64_t) / ARGUMENT(1, int64_t);
			break;
		}
		case KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES: {
			ARGUMENT(1, uint64_t) = ARGUMENT(0, uint64_t);
			break;
		}
		case KETL_INSTRUCTION_CODE_RETURN: {
			return;
		}
		}
		*pIndex = index += CODE_SIZE(instruction.code);
	}
}