//🍲ketl
#include "ketl/type.h"

uint64_t getStackTypeSize(KETLVariableTraits traits, KETLType* type) {
	// TODO!! not sure about ref, its not always shows proper stack size now
	return traits.type >= KETL_TRAIT_TYPE_REF || traits.isNullable ||
		type->kind == KETL_TYPE_KIND_CLASS || type->kind == KETL_TYPE_KIND_FUNCTION ? sizeof(void*) : type->size;
}