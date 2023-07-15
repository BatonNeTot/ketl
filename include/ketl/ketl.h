//🍲ketl
#ifndef ketl_h
#define ketl_h

#include "compiler/compiler.h"
#include "atomic_strings.h"

#include "utils.h"

KETL_DEFINE(KETLState) {
	KETLAtomicStrings strings;
	KETLCompiler compiler;
};

void ketlInitState(KETLState* state);

void ketlDeinitState(KETLState* state);

#endif /*ketl_h*/
