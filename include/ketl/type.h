//🫖ketl
#ifndef ketl_type_h
#define ketl_type_h

#include "ketl/utils.h"


ANN_FORWARD(ketl_type);

ANN_DEFINE(ketl_type_parameter) {
	ketl_type* pType;
};

ANN_DEFINE(ketl_function_parameters) {
    ketl_type_parameter* pParameters;
    uint16_t parametersCount;
};

#endif // ketl_type_h
