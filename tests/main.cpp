/*🍲Ketl🍲*/
#include <iostream>

extern "C" {
#include "ketl/ketl.h"
#include "ketl/compiler/syntax_solver.h"
#include "compiler/ir_node.h"
#include "compiler/ir_compiler.h"
#include "ketl/function.h"
}

template <typename... Args>
constexpr KETLInstruction CODE(KETLInstructionCode code, Args... args) {
	KETLInstruction instruction;
	instruction.code = code;
	if constexpr (sizeof...(args) > 0) {
		KETLInstructionArgumentType list[] = { static_cast<KETLInstructionArgumentType>(args)... };
		for (auto i = 0; i < sizeof...(args); ++i) {
			instruction.argumentTypes[i + 1] = list[0];
		}
	}
	return instruction;
}

constexpr KETLInstruction GLOBAL(void* ptr) {
	KETLInstruction instruction;
	instruction.argument.globalPtr = ptr;
	return instruction;
}

int a = 5;
int b = 10;
int c = 0;

const KETLInstruction templateInstructions[] = {
	CODE(KETL_INSTRUCTION_CODE_ADD_INT64, KETL_INSTRUCTION_ARGUMENT_TYPE_GLOBAL, KETL_INSTRUCTION_ARGUMENT_TYPE_GLOBAL, KETL_INSTRUCTION_ARGUMENT_TYPE_GLOBAL),
	GLOBAL(&a),
	GLOBAL(&b),
	GLOBAL(&c),
	CODE(KETL_INSTRUCTION_CODE_RETURN)
};

int main(int argc, char** argv) {
	
	auto source = "return 1 + 2;";

	/*
	KETLFunction* function = reinterpret_cast<KETLFunction*>(malloc(sizeof(KETLFunction) + sizeof(templateInstructions)));

	KETLInstruction* instructions = reinterpret_cast<KETLInstruction*>(function + 1);

	memcpy(instructions, templateInstructions, sizeof(templateInstructions));

	uint64_t index;
	ketlCallFunction(function, &index, NULL);
	*/

	KETLState ketlState;

	ketlInitState(&ketlState);

	auto root = ketlSolveSyntax(source, KETL_NULL_TERMINATED_LENGTH, &ketlState.compiler.syntaxSolver, &ketlState.compiler.syntaxNodePool);

	KETLIRState irState;

	ketlBuildIR(&ketlState.compiler.irBuilder, &irState, root);

	// TODO optimization on ir

	KETLFunction* function = ketlCompileIR(&irState);

	uint64_t result = 0;
	uint8_t* stack = new uint8_t[function->stackSize + sizeof(uint64_t)];
	ketlCallFunction(function, stack, &result);

	ketlDeinitState(&ketlState);

	return 0;
}