/*🍲Ketl🍲*/
#include <iostream>

extern "C" {
#include "compiler/syntax_solver.h"
#include "compiler/syntax_node.h"
#include "ketl/object_pool.h"
}

#include <crtdbg.h>

int main(int argc, char** argv) {
	
	auto source = "let test1 := 5 + 10; let test2 := test1 - 17;";

	KETLSyntaxSolver syntaxSolver;
	ketlInitSyntaxSolver(&syntaxSolver);

	KETLObjectPool syntaxNodePool;
	ketlInitObjectPool(&syntaxNodePool, sizeof(KETLSyntaxNode), 16);

	auto* root = ketlSolveSyntax(source, KETL_NULL_TERMINATED_LENGTH, &syntaxSolver, &syntaxNodePool);

	ketlDeinitObjectPool(&syntaxNodePool);
	ketlDeinitSyntaxSolver(&syntaxSolver);

	return 0;
}