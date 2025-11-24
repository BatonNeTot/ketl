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

void ketl_state_destroy(ketl_state* p_state);

ketl_type* ketl_state_get_none_type(ketl_state* p_state);
ketl_type* ketl_state_get_raw_type(ketl_state* p_state);

ketl_type* ketl_state_get_i8(ketl_state* p_state);
ketl_type* ketl_state_get_i16(ketl_state* p_state);
ketl_type* ketl_state_get_i32(ketl_state* p_state);
ketl_type* ketl_state_get_i64(ketl_state* p_state);

ketl_type* ketl_state_get_type(ketl_state* p_state, const char* p_type_name, uint32_t length);

ketl_type* ketl_state_get_array_type(ketl_state* p_state, ketl_type* p_type);

ketl_type* ketl_state_get_function_type(ketl_state* p_state, const ketl_function_parameters* p_parameters);

ketl_type* ketl_state_get_cfunction_type(ketl_state* p_state, const ketl_function_parameters* p_parameters);

void ketl_state_define_function(ketl_state* p_state, const char* p_name, uint32_t length, ketl_type* p_type, void* p_func);

void ketl_state_define_class(ketl_state* p_state, const char* p_name, uint32_t length, ketl_named_variable_type_info_t* p_fields, uint16_t field_count);

ketl_value* ketl_state_eval(ketl_state* p_state, const char* p_filename, const char* p_source, uint32_t length);

#endif // ketl_ketl_h
