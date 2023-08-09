//🍲ketl
#include "compiler/ir_compiler.h"

#include "compiler/ir_node.h"
#include "compiler/ir_builder.h"
#include "ketl/function.h"
#include "ketl/type.h"

#include <stdlib.h>

static inline uint64_t bakeStackUsage(KETLIRState* irState) {
	// TODO align
	uint64_t currentStackOffset = 0;
	uint64_t maxStackOffset = 0;

	KETLIRValue* it = irState->stackRoot;
	if (it == NULL) {
		return maxStackOffset;
	}

	KETL_FOREVER {
		it->argument.stack = currentStackOffset;

		uint64_t size = getStackTypeSize(it->traits, it->type);
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
				size = getStackTypeSize(it->traits, it->type);
				currentStackOffset -= size;
			}
			it = it->nextSibling;
		}
	}
}

static inline uint64_t calculateInstructionSizes(KETLIRState* irState) {
	uint64_t instructionCount = 0;

	KETLIRInstruction* it = irState->first;
	while (it) {
		it->instructionOffset = instructionCount;
		instructionCount += KETL_CODE_SIZE(it->code);
		it = it->next;
	}

	return instructionCount;
}

static inline void bakeJumps(KETLIRState* irState) {
	KETLIRInstruction* it = irState->first;
	while (it) {
		if (it->code >= KETL_INSTRUCTION_CODE_JUMP && it->code <= KETL_INSTRUCTION_CODE_JUMP_IF_FALSE) {
			KETLIRValue* value = it->arguments[0];
			value->argument.uint64 = ((KETLIRInstruction*)value->argument.globalPtr)->instructionOffset - it->instructionOffset;
		}
		it = it->next;
	}
}

KETLFunction* ketlCompileIR(KETLIRState* irState) {
	uint64_t maxStackOffset = bakeStackUsage(irState);
	uint64_t instructionCount = calculateInstructionSizes(irState);
	bakeJumps(irState);

	KETLFunction* function = malloc(sizeof(KETLFunction) + sizeof(KETLInstruction) * instructionCount);
	function->stackSize = maxStackOffset;
	function->instructionsCount = instructionCount;

	KETLInstruction* instructions = (KETLInstruction*)(function + 1);

	uint64_t instructionIndex = 0;
	{
		KETLIRInstruction* it = irState->first;
		for (; it; it = it->next) {
			KETLIRInstruction irInstruction = *it;
			if (irInstruction.code == KETL_INSTRUCTION_CODE_NONE) {
				continue;
			}

			uint64_t instructionSize = KETL_CODE_SIZE(irInstruction.code);

			KETLInstruction instruction;
			instruction.code = irInstruction.code;

			for (uint64_t i = 1; i < instructionSize; ++i) {
				instructions[instructionIndex + i].argument = irInstruction.arguments[i - 1]->argument;
				instruction.argumentTraits[i + (KETL_INSTRUCTION_RESERVED_ARGUMENT_TRAITS_COUNT - 1)] = irInstruction.arguments[i - 1]->argTraits;
			}

			instructions[instructionIndex] = instruction;
			instructionIndex += instructionSize;
		}
	}

	return function;
}