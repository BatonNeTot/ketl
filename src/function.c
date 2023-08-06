//🍲ketl
#include "ketl/function.h"

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
(*(type*)(getArgument(stackPtr, \
instruction.argumentTypes[(number) + KETL_INSTRUCTION_RESERVED_ARGUMENTS_COUNT], \
&(pInstruction + (number) + 1)->argument)))


void ketlCallFunction(KETLFunction* function, void* _stackPtr, void* returnPtr) {
	uint64_t* pIndex = (uint64_t*)(_stackPtr);
	uint64_t index = *pIndex = 0;
	uint8_t* stackPtr = ((uint8_t*)(_stackPtr)) + sizeof(index);
	KETL_FOREVER {
		KETLInstruction* pInstruction = ((KETLInstruction*)(function + 1)) + index;
		KETLInstruction instruction = *pInstruction;
		switch (instruction.code) {

		case KETL_INSTRUCTION_CODE_CAST_INT8_INT16:
			ARGUMENT(0, int16_t) = ARGUMENT(1, int8_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT8_INT32:
			ARGUMENT(0, int32_t) = ARGUMENT(1, int8_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT8_INT64:
			ARGUMENT(0, int64_t) = ARGUMENT(1, int8_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT8_FLOAT32:
			ARGUMENT(0, float) = ARGUMENT(1, int8_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT8_FLOAT64:
			ARGUMENT(0, double) = ARGUMENT(1, int8_t);
			break;

		case KETL_INSTRUCTION_CODE_CAST_INT16_INT8:
			ARGUMENT(0, int8_t) = (int8_t)ARGUMENT(1, int16_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT16_INT32:
			ARGUMENT(0, int32_t) = ARGUMENT(1, int16_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT16_INT64:
			ARGUMENT(0, int64_t) = ARGUMENT(1, int16_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT16_FLOAT32:
			ARGUMENT(0, float) = ARGUMENT(1, int16_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT16_FLOAT64:
			ARGUMENT(0, double) = ARGUMENT(1, int16_t);
			break;

		case KETL_INSTRUCTION_CODE_CAST_INT32_INT8:
			ARGUMENT(0, int8_t) = (int8_t)ARGUMENT(1, int32_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT32_INT16:
			ARGUMENT(0, int16_t) = (int16_t)ARGUMENT(1, int32_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT32_INT64:
			ARGUMENT(0, int64_t) = ARGUMENT(1, int32_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT32_FLOAT32:
			ARGUMENT(0, float) = (float)ARGUMENT(1, int32_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT32_FLOAT64:
			ARGUMENT(0, double) = ARGUMENT(1, int32_t);
			break;

		case KETL_INSTRUCTION_CODE_CAST_INT64_INT8:
			ARGUMENT(0, int8_t) = (int8_t)ARGUMENT(1, int64_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT64_INT16:
			ARGUMENT(0, int16_t) = (int16_t)ARGUMENT(1, int64_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT64_INT32:
			ARGUMENT(0, int32_t) = (int32_t)ARGUMENT(1, int64_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT64_FLOAT32:
			ARGUMENT(0, float) = (float)ARGUMENT(1, int64_t);
			break;
		case KETL_INSTRUCTION_CODE_CAST_INT64_FLOAT64:
			ARGUMENT(0, double) = (double)ARGUMENT(1, int64_t);
			break;

		case KETL_INSTRUCTION_CODE_CAST_FLOAT32_INT8:
			ARGUMENT(0, int8_t) = (int8_t)ARGUMENT(1, float);
			break;
		case KETL_INSTRUCTION_CODE_CAST_FLOAT32_INT16:
			ARGUMENT(0, int16_t) = (int16_t)ARGUMENT(1, float);
			break;
		case KETL_INSTRUCTION_CODE_CAST_FLOAT32_INT32:
			ARGUMENT(0, int32_t) = (int32_t)ARGUMENT(1, float);
			break;
		case KETL_INSTRUCTION_CODE_CAST_FLOAT32_INT64:
			ARGUMENT(0, int64_t) = (int64_t)ARGUMENT(1, float);
			break;
		case KETL_INSTRUCTION_CODE_CAST_FLOAT32_FLOAT64:
			ARGUMENT(0, double) = ARGUMENT(1, float);
			break;

		case KETL_INSTRUCTION_CODE_CAST_FLOAT64_INT8:
			ARGUMENT(0, int8_t) = (int8_t)ARGUMENT(1, double);
			break;
		case KETL_INSTRUCTION_CODE_CAST_FLOAT64_INT16:
			ARGUMENT(0, int16_t) = (int16_t)ARGUMENT(1, double);
			break;
		case KETL_INSTRUCTION_CODE_CAST_FLOAT64_INT32:
			ARGUMENT(0, int32_t) = (int32_t)ARGUMENT(1, double);
			break;
		case KETL_INSTRUCTION_CODE_CAST_FLOAT64_INT64:
			ARGUMENT(0, int64_t) = (int64_t)ARGUMENT(1, double);
			break;
		case KETL_INSTRUCTION_CODE_CAST_FLOAT64_FLOAT32:
			ARGUMENT(0, float) = (float)ARGUMENT(1, double);
			break;

		case KETL_INSTRUCTION_CODE_ADD_INT8:
			ARGUMENT(0, int8_t) = ARGUMENT(1, int8_t) + ARGUMENT(2, int8_t);
			break;
		case KETL_INSTRUCTION_CODE_ADD_INT16:
			ARGUMENT(0, int16_t) = ARGUMENT(1, int16_t) + ARGUMENT(2, int16_t);
			break;
		case KETL_INSTRUCTION_CODE_ADD_INT32:
			ARGUMENT(0, int32_t) = ARGUMENT(1, int32_t) + ARGUMENT(2, int32_t);
			break;
		case KETL_INSTRUCTION_CODE_ADD_INT64:
			ARGUMENT(0, int64_t) = ARGUMENT(1, int64_t) + ARGUMENT(2, int64_t);
			break;
		case KETL_INSTRUCTION_CODE_ADD_FLOAT32:
			ARGUMENT(0, float) = ARGUMENT(1, float) + ARGUMENT(2, float);
			break;
		case KETL_INSTRUCTION_CODE_ADD_FLOAT64:
			ARGUMENT(0, double) = ARGUMENT(1, double) + ARGUMENT(2, double);
			break;

		case KETL_INSTRUCTION_CODE_SUB_INT8:
			ARGUMENT(0, int8_t) = ARGUMENT(1, int8_t) - ARGUMENT(2, int8_t);
			break;
		case KETL_INSTRUCTION_CODE_SUB_INT16:
			ARGUMENT(0, int16_t) = ARGUMENT(1, int16_t) - ARGUMENT(2, int16_t);
			break;
		case KETL_INSTRUCTION_CODE_SUB_INT32:
			ARGUMENT(0, int32_t) = ARGUMENT(1, int32_t) - ARGUMENT(2, int32_t);
			break;
		case KETL_INSTRUCTION_CODE_SUB_INT64:
			ARGUMENT(0, int64_t) = ARGUMENT(1, int64_t) - ARGUMENT(2, int64_t);
			break;
		case KETL_INSTRUCTION_CODE_SUB_FLOAT32:
			ARGUMENT(0, float) = ARGUMENT(1, float) - ARGUMENT(2, float);
			break;
		case KETL_INSTRUCTION_CODE_SUB_FLOAT64:
			ARGUMENT(0, double) = ARGUMENT(1, double) - ARGUMENT(2, double);
			break;

		case KETL_INSTRUCTION_CODE_MULTY_INT8:
			ARGUMENT(0, int8_t) = ARGUMENT(1, int8_t) * ARGUMENT(2, int8_t);
			break;
		case KETL_INSTRUCTION_CODE_MULTY_INT16:
			ARGUMENT(0, int16_t) = ARGUMENT(1, int16_t) * ARGUMENT(2, int16_t);
			break;
		case KETL_INSTRUCTION_CODE_MULTY_INT32:
			ARGUMENT(0, int32_t) = ARGUMENT(1, int32_t) * ARGUMENT(2, int32_t);
			break;
		case KETL_INSTRUCTION_CODE_MULTY_INT64:
			ARGUMENT(0, int64_t) = ARGUMENT(1, int64_t) * ARGUMENT(2, int64_t);
			break;
		case KETL_INSTRUCTION_CODE_MULTY_FLOAT32:
			ARGUMENT(0, float) = ARGUMENT(1, float) * ARGUMENT(2, float);
			break;
		case KETL_INSTRUCTION_CODE_MULTY_FLOAT64:
			ARGUMENT(0, double) = ARGUMENT(1, double) * ARGUMENT(2, double);
			break;

		case KETL_INSTRUCTION_CODE_DIV_INT8:
			ARGUMENT(0, int8_t) = ARGUMENT(1, int8_t) / ARGUMENT(2, int8_t);
			break;
		case KETL_INSTRUCTION_CODE_DIV_INT16:
			ARGUMENT(0, int16_t) = ARGUMENT(1, int16_t) / ARGUMENT(2, int16_t);
			break;
		case KETL_INSTRUCTION_CODE_DIV_INT32:
			ARGUMENT(0, int32_t) = ARGUMENT(1, int32_t) / ARGUMENT(2, int32_t);
			break;
		case KETL_INSTRUCTION_CODE_DIV_INT64:
			ARGUMENT(0, int64_t) = ARGUMENT(1, int64_t) / ARGUMENT(2, int64_t);
			break;
		case KETL_INSTRUCTION_CODE_DIV_FLOAT32:
			ARGUMENT(0, float) = ARGUMENT(1, float) / ARGUMENT(2, float);
			break;
		case KETL_INSTRUCTION_CODE_DIV_FLOAT64:
			ARGUMENT(0, double) = ARGUMENT(1, double) / ARGUMENT(2, double);
			break;

		case KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES: {
			ARGUMENT(0, uint64_t) = ARGUMENT(1, uint64_t);
			break;
		}
		case KETL_INSTRUCTION_CODE_RETURN: {
			return;
		}
		case KETL_INSTRUCTION_CODE_RETURN_8_BYTES: {
			*((uint64_t*)returnPtr) = ARGUMENT(0, uint64_t);
			return;
		}
		}
		*pIndex = index += KETL_CODE_SIZE(instruction.code);
	}
}