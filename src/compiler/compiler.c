//🍲ketl
#include "ketl/compiler/compiler.h"

#include "syntax_node.h"

void ketlInitCompiler(KETLCompiler* compiler) {
	ketlInitSyntaxSolver(&compiler->syntaxSolver);
	ketlInitObjectPool(&compiler->syntaxNodePool, sizeof(KETLSyntaxNode), 16);
}

void ketlDeinitCompiler(KETLCompiler* compiler) {
	ketlDeinitObjectPool(&compiler->syntaxNodePool);
	ketlDeinitSyntaxSolver(&compiler->syntaxSolver);
}