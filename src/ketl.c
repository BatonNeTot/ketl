//🍲ketl
#include "ketl/ketl.h"

void ketlInitState(KETLState* state) {
	ketlInitAtomicStrings(&state->strings, 16);
	ketlInitCompiler(&state->compiler);
}

void ketlDeinitState(KETLState* state) {
	ketlDeinitCompiler(&state->compiler);
	ketlDeinitAtomicStrings(&state->strings);
}