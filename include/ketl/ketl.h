//🫖ketl
#ifndef ketl_ketl_h
#define ketl_ketl_h

#include "ketl/value.h"
#include "ketl/memory.h"
#include "ketl/type.h"
#include "ketl/function.h"

#include "ketl/utils.h"

ANN_FORWARD(ketl_state);

ketl_state* ketl_state_create(const ketl_allocator* p_allocator);

void ketl_state_destroy(ketl_state* pState);

ketl_type* ketl_state_get_none_type(ketl_state* pState);

ketl_type* ketl_state_get_i64(ketl_state* pState);

ketl_type* ketl_state_get_type(ketl_state* p_state, const char* p_type_name, uint32_t length);

ketl_type* ketl_state_get_function_type(ketl_state* pState, const ketl_function_parameters* pParameters);

ketl_type* ketl_state_get_cfunction_type(ketl_state* pState, const ketl_function_parameters* pParameters);

void ketl_state_define_function(ketl_state* pState, const char* pName, uint32_t length, ketl_type* pType, void* pFunc);

ketl_value* ketl_state_eval(ketl_state* pState, const char* p_filename, const char* p_source, uint32_t length);

#endif // ketl_ketl_h
