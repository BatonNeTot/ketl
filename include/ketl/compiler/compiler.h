//🍲ketl
#ifndef compiler_compiler_h
#define compiler_compiler_h

#include "syntax_solver.h"
#include "compiler/ir_builder.h"

#include "ketl/utils.h"

KETL_DEFINE(KETLCompiler) {
	KETLObjectPool syntaxNodePool;
	KETLSyntaxSolver syntaxSolver;
	KETLIRBuilder irBuilder;
};

void ketlInitCompiler(KETLCompiler* compiler);

void ketlDeinitCompiler(KETLCompiler* compiler);

#endif /*compile_compiler_h*/
