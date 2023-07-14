/*🍲Ketl🍲*/
#include <iostream>

extern "C" {
#include "ketl/ketl.h"
#include "ketl/compiler/syntax_solver.h"
#include "compiler/ir_node.h"
#include "compiler/ir_compiler.h"
#include "ketl/function.h"
#include "ketl/atomic_strings.h"
#include "ketl/int_map.h"
}

int main(int argc, char** argv) {	
	auto source = "return 1 + 2;";

	KETLState ketlState;

	ketlInitState(&ketlState);

	auto root = ketlSolveSyntax(source, KETL_NULL_TERMINATED_LENGTH, &ketlState.compiler.syntaxSolver, &ketlState.compiler.syntaxNodePool);

	KETLIRState irState;

	ketlBuildIR(nullptr, &ketlState.compiler.irBuilder, &irState, root);

	// TODO optimization on ir

	KETLFunction* function = ketlCompileIR(&irState);

	uint64_t result = 0;
	uint8_t* stack = new uint8_t[function->stackSize + sizeof(uint64_t)];
	ketlCallFunction(function, stack, &result);

	ketlDeinitState(&ketlState);

	return 0;
}
/*
std::string test(bool test) {
	if (test) {
		return "";
	}
}
*/