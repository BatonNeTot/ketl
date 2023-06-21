//🍲ketl
#ifndef compiler_syntax_solver_h
#define compiler_syntax_solver_h

#include "ketl/common.h"
#include "ketl/object_pool.h"
#include "ketl/stack.h"

KETL_FORWARD(KETLSyntaxNode);

KETL_FORWARD(KETLToken);
KETL_FORWARD(KETLBnfNode);

KETL_FORWARD(KETLSyntaxSolver);

struct KETLSyntaxSolver {
	KETLObjectPool tokenPool;
	KETLObjectPool bnfNodePool;
	KETLStack bnfStateStack;
	KETLBnfNode* bnfScheme;
};

void ketlInitSyntaxSolver(KETLSyntaxSolver* syntaxSolver);

void ketlDeinitSyntaxSolver(KETLSyntaxSolver* syntaxSolver);

KETLSyntaxNode* ketlSolveSyntax(const char* source, size_t length, KETLSyntaxSolver* syntaxSolver, KETLObjectPool* syntaxNodePool);

#endif /*compiler_bnf_solver_h*/
