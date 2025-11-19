//🫖ketl
#ifndef ketl_type_h
#define ketl_type_h

#include "ketl/utils.h"


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

#endif // ketl_type_h
