//🍲ketl
#ifndef compiler_bytecode_compiler_h
#define compiler_bytecode_compiler_h

#include "syntax_solver.h"

#include "ketl/utils.h"

KETL_FORWARD(KETLState);

KETL_DEFINE(KETLBytecodeCompiler) {
	KETLObjectPool syntaxNodePool;
	KETLSyntaxSolver syntaxSolver;
};

void ketlInitBytecodeCompiler(KETLBytecodeCompiler* compiler);

void ketlDeinitBytecodeCompiler(KETLBytecodeCompiler* compiler);

#endif /*compiler_bytecode_compiler_h*/
