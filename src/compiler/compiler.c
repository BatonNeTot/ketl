//🍲ketl
#include "ketl/compiler/compiler.h"

void ketlInitCompiler(KETLCompiler* compiler, KETLState* state) {
	ketlInitBytecodeCompiler(&compiler->bytecodeCompiler);
	ketlInitIRBuilder(&compiler->irBuilder, state);
}

void ketlDeinitCompiler(KETLCompiler* compiler) {
	ketlDeinitIRBuilder(&compiler->irBuilder);
	ketlDeinitBytecodeCompiler(&compiler->bytecodeCompiler);
}