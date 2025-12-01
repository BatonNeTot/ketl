//🫖ketl
#ifndef ketl_type_h
#define ketl_type_h

#include "ketl/utils.h"

ANN_FORWARD(ketl_state);
ANN_FORWARD(ketl_type);

ANN_DEFINE(ketl_variable_type_info_t) {
	ketl_type* p_type;
};

ANN_DEFINE(ketl_function_parameters) {
    ketl_variable_type_info_t* p_parameters;
    uint16_t parameters_count;
};

ANN_DEFINE(ketl_named_variable_type_info_t) {
	ketl_variable_type_info_t info;
    const char* p_name;
    uint32_t name_length;
};

size_t ketl_type_get_size(ketl_type* p_type);

size_t ketl_type_get_align(ketl_type* p_type);

bool ketl_type_is_array(ketl_type* p_type);

uint32_t ketl_type_format(ketl_state* p_state, ketl_type* p_type, char* p_buffer, uint32_t buffer_size);

#endif // ketl_type_h
