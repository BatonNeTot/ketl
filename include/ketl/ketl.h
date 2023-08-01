//🍲ketl
#ifndef ketl_h
#define ketl_h

#include "compiler/compiler.h"
#include "atomic_strings.h"
#include "object_pool.h"
#include "int_map.h"
#include "type.h"
#include "instructions.h"
#include "operators.h"
#include "utils.h"

KETL_FORWARD(KETLType);

KETL_DEFINE(KETLNamespace) {
	KETLIntMap variables;
	KETLIntMap namespaces;
	KETLIntMap types;
};

KETL_DEFINE(KETLUnaryOperator) {
	KETLInstructionCode code;
	KETLVariableTraits inputTraits;
	KETLVariableTraits outputTraits;
	KETLType* inputType;
	KETLType* outputType;
	KETLUnaryOperator* next;
};

KETL_DEFINE(KETLBinaryOperator) {
	KETLInstructionCode code;
	KETLVariableTraits lhsTraits;
	KETLVariableTraits rhsTraits;
	KETLVariableTraits outputTraits;
	KETLType* lhsType;
	KETLType* rhsType;
	KETLType* outputType;
	KETLBinaryOperator* next;
};

KETL_DEFINE(KETLCastOperator) {
	KETLInstructionCode code;
	bool implicit;
	KETLVariableTraits inputTraits;
	KETLVariableTraits outputTraits;
	KETLType* outputType;
	KETLCastOperator* next;
};

KETL_DEFINE(KETLState) {
	KETLAtomicStrings strings;
	KETLCompiler compiler;
	KETLNamespace globalNamespace;
	KETLType* primitives[6];

	KETLObjectPool unaryOperatorsPool;
	KETLObjectPool binaryOperatorsPool;
	KETLObjectPool castOperatorsPool;
	KETLIntMap unaryOperators;
	KETLIntMap binaryOperators;
	KETLIntMap castOperators;
};

void ketlInitState(KETLState* state);

void ketlDeinitState(KETLState* state);

#endif /*ketl_h*/
