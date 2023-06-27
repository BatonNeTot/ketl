//🍲ketl
#include "ir_builder.h"

#include "syntax_node.h"
#include "ir_node.h"

#include "ketl/object_pool.h"

static void buildIrExpression();

KETLIRCommand* ketlBuildIR(KETLSyntaxNode* syntaxNodeRoot, KETLObjectPool* irCommandPool, KETLObjectPool* irExpressionPool) {
	KETLSyntaxNode* it = syntaxNodeRoot;
	while (it) {
		switch (it->type) {
		case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR: {
			KETLIRCommand* command = ketlGetFreeObjectFromPool(irCommandPool);
			command->type = KETL_IR_COMMAND_TYPE_DEFINE_VAR;

			__debugbreak(); 
		}
		default:
			__debugbreak();
		}
	}
	return NULL;
}