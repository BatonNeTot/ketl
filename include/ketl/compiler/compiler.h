//🍲ketl
#ifndef compiler_compiler_h
#define compiler_compiler_h

#include "syntax_solver.h"

#include "ketl/utils.h"

KETL_FORWARD(KETLCompiler);

struct KETLCompiler {
	KETLSyntaxSolver syntaxSolver;
	KETLObjectPool syntaxNodePool;
};

void ketlInitCompiler(KETLCompiler* compiler);

void ketlDeinitCompiler(KETLCompiler* compiler);

#endif /*compile_compiler_h*/
