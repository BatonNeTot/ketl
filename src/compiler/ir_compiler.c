//🍲ketl
#include "compiler/ir_compiler.h"

#include "compiler/ir_node.h"
#include "compiler/ir_builder.h"
#include "ketl/function.h"

#include <stdlib.h>

static inline uint64_t bakeStackUsage(KETLIRState* irState) {
	uint64_t currentStackOffset = 0;
	uint64_t maxStackOffset = 0;

	KETLIRValue* it = irState->stackRoot;
	if (it == NULL) {
		return maxStackOffset;
	}

	KETL_FOREVER {
		it->argument.stack = currentStackOffset;

		// TODO get size from type
		uint64_t size = 8;
		currentStackOffset += size;
		if (maxStackOffset < currentStackOffset) {
			maxStackOffset = currentStackOffset;
		}

		if (it->firstChild) {
			it = it->firstChild;
		}
		else {
			currentStackOffset -= size;
			while (it->nextSibling == NULL) {
				if (it->parent == NULL) {
					return maxStackOffset;
				}
				it = it->parent;
				// TODO get size from type
				size = 8;
				currentStackOffset -= size;
			}
			it = it->nextSibling;
		}
	}
}

static inline uint64_t calculateInstructionCount(KETLIRState* irState) {
	uint64_t instructionCount = 0;

	KETLIRInstruction* it = irState->first;
	while (it) {
		instructionCount += KETL_CODE_SIZE(it->code);
		it = it->next;
	}

	return instructionCount;
}

KETLFunction* ketlCompileIR(KETLIRState* irState) {
	uint64_t maxStackOffset = bakeStackUsage(irState);
	uint64_t instructionCount = calculateInstructionCount(irState);

	KETLFunction* function = malloc(sizeof(KETLFunction) + sizeof(KETLInstruction) * instructionCount);
	function->stackSize = maxStackOffset;
	function->instructionsCount = instructionCount;

	KETLInstruction* instructions = (KETLInstruction*)(function + 1);

	uint64_t instructionIndex = 0;
	{
		KETLIRInstruction* it = irState->first;
		while (it) {
			KETLIRInstruction irInstruction = *it;
			uint64_t instructionSize = KETL_CODE_SIZE(irInstruction.code);

			KETLInstruction instruction;
			instruction.code = irInstruction.code;

			for (uint64_t i = 1; i < instructionSize; ++i) {
				instructions[instructionIndex + i].argument = irInstruction.arguments[i - 1]->argument;
				instruction.argumentTypes[i] = irInstruction.arguments[i - 1]->argType;
			}

			instructions[instructionIndex] = instruction;
			instructionIndex += instructionSize;

			it = it->next;
		}
	}

	return function;
}