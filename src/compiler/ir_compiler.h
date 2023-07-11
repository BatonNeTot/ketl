//🍲ketl
#ifndef compiler_ir_compiler_h
#define compiler_ir_compiler_h

#include "ketl/utils.h"

KETL_FORWARD(KETLFunction);
KETL_FORWARD(KETLIRState);

KETLFunction* ketlCompileIR(KETLIRState* irState);

#endif /*compiler_ir_compiler_h*/
