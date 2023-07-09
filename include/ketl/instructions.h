//🍲ketl
#ifndef instructions_h
#define instructions_h

#include "utils.h"

#include <inttypes.h>

KETL_DEFINE(KETLInstructionArgument) {
	union {
		void* globalPtr;
		uint64_t stack;

		int64_t integer;
		uint64_t uinteger;
		double floating;
		void* pointer;
	};
};

typedef uint8_t KETLInstructionArgumentType;

#define KETL_INSTRUCTION_ARGUMENT_TYPE_NONE 0
#define KETL_INSTRUCTION_ARGUMENT_TYPE_GLOBAL 1
#define KETL_INSTRUCTION_ARGUMENT_TYPE_STACK 2
#define KETL_INSTRUCTION_ARGUMENT_TYPE_LITERAL 3
#define KETL_INSTRUCTION_ARGUMENT_TYPE_FUNCTION_PARAMETER 4

typedef uint8_t KETLInstructionCode;

#define KETL_INSTRUCTION_CODE_NONE 0

#define KETL_INSTRUCTION_CODE_ADD_INT64 1
#define KETL_INSTRUCTION_CODE_SUB_INT64 2
#define KETL_INSTRUCTION_CODE_MULTY_INT64 3
#define KETL_INSTRUCTION_CODE_DIV_INT64 4

#define KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES 5

#define KETL_INSTRUCTION_CODE_RETURN 6


KETL_DEFINE(KETLInstruction) {
	union {
		KETLInstructionCode code;
		KETLInstructionArgumentType argumentTypes[8]; // zero one reserved
		KETLInstructionArgument argument;
	};
};

#endif /*instructions_h*/
