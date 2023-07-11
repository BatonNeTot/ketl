/*🍲Ketl🍲*/
#include <iostream>

extern "C" {
#include "ketl/ketl.h"
#include "ketl/compiler/syntax_solver.h"
#include "compiler/ir_node.h"
#include "compiler/ir_builder.h"
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
	
	auto source = "{let test1 := 5 + 10; let test2 := test1 := test1 - 8; } let test3 := test2 - test1 + 1;";

	KETLFunction* function = reinterpret_cast<KETLFunction*>(malloc(sizeof(KETLFunction) + sizeof(templateInstructions)));

	KETLInstruction* instructions = reinterpret_cast<KETLInstruction*>(function + 1);

	memcpy(instructions, templateInstructions, sizeof(templateInstructions));

	uint64_t index;
	ketlCallFunction(function, &index, NULL);

	KETLState ketlState;

	ketlInitState(&ketlState);

	auto root = ketlSolveSyntax(source, KETL_NULL_TERMINATED_LENGTH, &ketlState.compiler.syntaxSolver, &ketlState.compiler.syntaxNodePool);

	KETLObjectPool irInstructionPool;

	ketlInitObjectPool(&irInstructionPool, sizeof(KETLIRInstruction), 16);

	KETLIRBuilder irBuilder;

	ketlInitIRBuilder(&irBuilder);

	KETLIRState irState;

	ketlBuildIR(&irBuilder, &irState, root);

	// TODO optimization on ir

	KETLFunction* compiledFunction = ketlCompileIR(&irState);

	ketlDeinitIRBuilder(&irBuilder);
	ketlDeinitState(&ketlState);

	return 0;
}