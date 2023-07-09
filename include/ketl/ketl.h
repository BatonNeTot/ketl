//🍲ketl
#ifndef ketl_h
#define ketl_h

#include "compiler/compiler.h"

#include "utils.h"

KETL_DEFINE(KETLState) {
	KETLCompiler compiler;
};

void ketlInitState(KETLState* state);

void ketlDeinitState(KETLState* state);

#endif /*ketl_h*/
