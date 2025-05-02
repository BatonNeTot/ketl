//🫖ketl
#ifndef ketl_type_impl_h
#define ketl_type_impl_h

#include "ketl/type.h"

#include "atomic_strings.h"

enum __KETL_TYPE {
KETL_TYPE_PRIMITIVE,
KETL_TYPE_FUNCTION,
KETL_TYPE_CFUNCTION,
};

#define KETL_TYPE_BODY \
ketl_atomic_string sName;\
uint8_t type;\
uint8_t align;\
uint16_t size

KETL_DEFINE(ketl_type) {
    KETL_TYPE_BODY;
};

KETL_DEFINE(ketl_type_primitive) {
    KETL_TYPE_BODY;
    bool isInteger;
    bool isSigned;
};

KETL_DEFINE(ketl_type_parameter) {
	ketl_type* pType;
};

KETL_DEFINE(ketl_type_function) {
    KETL_TYPE_BODY;
    uint16_t parametersCount;
	//ketl_type* pReturnType; return type is first parameter for now
    ketl_type_parameter aParameters[0];
};


#endif // ketl_type_impl_h
