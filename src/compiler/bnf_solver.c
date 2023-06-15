//🍲ketl
#include "bnf_solver.h"

#include "token.h"
#include "syntax_node.h"
#include "ketl/object_pool.h"

KETLSyntaxNode* ketlSolveBnf(KETLBnfNode* scheme, KETLObjectPool* syntaxNodePool) {
	KETLSyntaxNode* define = ketlGetFreeObjectFromPool(syntaxNodePool);
	define->type = KETL_SYNTAX_NODE_TYPE_DEFINE_VAR;
	define->value = "test";
	define->length = 5;
	define->nextSibling = NULL;

	KETLSyntaxNode* lit = ketlGetFreeObjectFromPool(syntaxNodePool);
	lit->type = KETL_SYNTAX_NODE_TYPE_NUMBER;
	lit->value = "0";
	lit->length = 2;
	lit->nextSibling = NULL;
	define->firstChild = lit;

	return define;
}
