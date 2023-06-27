//🍲ketl
#include "ketl/ketl.h"

void ketlInitState(KETLState* state) {
	ketlInitCompiler(&state->compiler);
}

void ketlDeinitState(KETLState* state) {
	ketlDeinitCompiler(&state->compiler);
}