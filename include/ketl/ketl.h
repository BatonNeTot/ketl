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

ketl_type* ketl_state_get_void(ketl_state* pState);

ketl_type* ketl_state_get_i64(ketl_state* pState);

ketl_type* ketl_state_get_function_type(ketl_state* pState, const ketl_function_parameters* pParameters);

ketl_type* ketl_state_get_cfunction_type(ketl_state* pState, const ketl_function_parameters* pParameters);

void ketl_state_define_function(ketl_state* pState, const char* pName, uint32_t length, ketl_type* pType, void* pFunc);

void ketl_state_eval(ketl_state* pState, const char* pSource, uint32_t length);

int64_t ketl_state_eval_int64(ketl_state* pState, const char* pSource, uint32_t length);

#endif // ketl_ketl_h
