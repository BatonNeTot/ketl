//🍲ketl
#ifndef compiler_ir_builder_h
#define compiler_ir_builder_h

#include "ketl/utils.h"

KETL_FORWARD(KETLIRCommand);
KETL_FORWARD(KETLSyntaxNode);
KETL_FORWARD(KETLObjectPool);

KETLIRCommand* ketlBuildIR(KETLSyntaxNode* syntaxNodeRoot, KETLObjectPool* irCommandPool, KETLObjectPool* irExpressionPool);

#endif /*compiler_ir_builder_h*/
