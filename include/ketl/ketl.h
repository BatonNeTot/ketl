//🫖ketl
#ifndef ketl_ketl_h
#define ketl_ketl_h

#include "ketl/memory.h"
#include "ketl/type.h"
#include "ketl/variable.h"
#include "ketl/function.h"

#include "ketl/utils.h"

KETL_FORWARD(ketl_state);

ketl_state* ketl_state_create(const ketl_allocator* pAllocator);

void ketl_state_destroy(ketl_state* pState);

void ketl_state_eval(ketl_state* pState, const char* pSource, uint32_t length);

int64_t ketl_state_eval_int64(ketl_state* pState, const char* pSource, uint32_t length);

#endif // ketl_ketl_h
