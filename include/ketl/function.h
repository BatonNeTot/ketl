//🍲ketl
#ifndef function_h
#define function_h

#include "instructions.h"
#include "utils.h"

#include <inttypes.h>

KETL_FORWARD(KETLFunction);

struct KETLFunction {
	uint64_t stackSize;
	uint64_t instructionsCount;
};

void ketlCallFunction(KETLFunction* function, void* stackPtr, void* returnPtr);

#endif /*function_h*/
