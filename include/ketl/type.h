//🫖ketl
#ifndef ketl_type_h
#define ketl_type_h

#include "ketl/utils.h"


ANN_FORWARD(ketl_type);

ANN_DEFINE(ketl_type_parameter) {
	ketl_type* p_type;
};

ANN_DEFINE(ketl_function_parameters) {
    ketl_type_parameter* pParameters;
    uint16_t parametersCount;
};

size_t ketl_type_get_size(ketl_type* p_type);

#endif // ketl_type_h
