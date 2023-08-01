﻿//🍲ketl
#include "ketl/compiler/compiler.h"

#include "syntax_node.h"

void ketlInitCompiler(KETLCompiler* compiler, KETLState* state) {
	ketlInitObjectPool(&compiler->syntaxNodePool, sizeof(KETLSyntaxNode), 16);
	ketlInitSyntaxSolver(&compiler->syntaxSolver);
	ketlInitIRBuilder(&compiler->irBuilder, state);
}

void ketlDeinitCompiler(KETLCompiler* compiler) {
	ketlDeinitIRBuilder(&compiler->irBuilder);
	ketlDeinitSyntaxSolver(&compiler->syntaxSolver);
	ketlDeinitObjectPool(&compiler->syntaxNodePool);
}