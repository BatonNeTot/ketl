//🍲ketl
#ifndef type_h
#define type_h

#include "int_map.h"
#include "utils.h"

typedef uint8_t KETLTypeKind;

#define KETL_TYPE_KIND_PRIMITIVE 0
#define KETL_TYPE_KIND_STRUCT 1
#define KETL_TYPE_KIND_CLASS 2
#define KETL_TYPE_KIND_FUNCTION 3

KETL_DEFINE(KETLType) {
	KETLIntMap variables;
	KETLTypeKind kind;
	const char* name;
	uint64_t size;
};

typedef uint8_t KETLTraitType;

#define KETL_TRAIT_TYPE_LITERAL 0
#define KETL_TRAIT_TYPE_LVALUE 1
#define KETL_TRAIT_TYPE_RVALUE 2
#define KETL_TRAIT_TYPE_REF 3
#define KETL_TRAIT_TYPE_REF_IN 4

KETL_DEFINE(KETLVariableTraits) {
	KETLTraitType type;
	bool isNullable;
	bool isConst;
};

KETL_DEFINE(KETLTypeVariable) {
	uint64_t offset;
	KETLType* type;
	KETLVariableTraits traits;
};

uint64_t getStackTypeSize(KETLVariableTraits traits, KETLType* type);

#endif /*type_h*/
