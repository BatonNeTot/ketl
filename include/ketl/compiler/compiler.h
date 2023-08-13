//🍲ketl
#ifndef compiler_compiler_h
#define compiler_compiler_h

#include "compiler/ir_builder.h"
#include "ketl/compiler/bytecode_compiler.h"

#include "ketl/utils.h"

KETL_FORWARD(KETLState);

KETL_DEFINE(KETLCompiler) {
	KETLBytecodeCompiler bytecodeCompiler;
	KETLIRBuilder irBuilder;
};

void ketlInitCompiler(KETLCompiler* compiler, KETLState* state);

void ketlDeinitCompiler(KETLCompiler* compiler);

#endif /*compile_compiler_h*/
