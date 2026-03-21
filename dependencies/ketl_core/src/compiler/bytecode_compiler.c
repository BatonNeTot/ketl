//🍲ketl
#include "ketl/compiler/bytecode_compiler.h"

#include "ketl/compiler/syntax_node.h"

void ketlInitBytecodeCompiler(KETLBytecodeCompiler* compiler) {
	ketlInitObjectPool(&compiler->syntaxNodePool, sizeof(KETLSyntaxNode), 16);
	ketlInitSyntaxSolver(&compiler->syntaxSolver);
}

void ketlDeinitBytecodeCompiler(KETLBytecodeCompiler* compiler) {
	ketlDeinitSyntaxSolver(&compiler->syntaxSolver);
	ketlDeinitObjectPool(&compiler->syntaxNodePool);
}