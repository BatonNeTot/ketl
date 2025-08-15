//🫖ketl
#ifndef ketl_value_h
#define ketl_value_h

#include "ketl/utils.h"

#include "ketl/type.h"


KETL_FORWARD(ketl_state);

KETL_FORWARD(ketl_value);

bool ketl_value_is_none(ketl_state* p_state, ketl_value* p_value);

ketl_type* ketl_value_get_type(ketl_state* p_state, ketl_value* p_value);

int64_t ketl_value_as_i64(ketl_state* p_state, ketl_value* p_value);

void ketl_value_destroy(ketl_state* p_state, ketl_value* p_value);

#endif
