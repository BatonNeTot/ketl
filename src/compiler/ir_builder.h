//🍲ketl
#ifndef compiler_ir_builder_h
#define compiler_ir_builder_h

#include "ketl/utils.h"

KETL_FORWARD(KETLIRInstruction);
KETL_FORWARD(KETLSyntaxNode);
KETL_FORWARD(KETLObjectPool);

KETLIRInstruction* ketlBuildIR(KETLSyntaxNode* syntaxNodeRoot, KETLObjectPool* irInstructionPool);

#endif /*compiler_ir_builder_h*/
