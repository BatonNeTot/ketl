//🫖ketl
#ifndef ketl_type_impl_h
#define ketl_type_impl_h

#include "ketl/type.h"

#include "atomic_strings.h"

enum {
    KETL_TYPE_PRIMITIVE,
    KETL_TYPE_FUNCTION,
    KETL_TYPE_CFUNCTION,
};

#define KETL_TYPE_BODY \
uint8_t type;\
uint8_t align;\
uint16_t size

ANN_DEFINE(ketl_type) {
    KETL_TYPE_BODY;
};

ANN_DEFINE(ketl_type_primitive) {
    KETL_TYPE_BODY;
    ketl_atomic_string sName;
    bool isInteger;
    bool isSigned;
};

ANN_DEFINE(ketl_type_signature) {
    uint16_t parametersCount;
	//ketl_type* pReturnType; return type is first parameter for now
    ketl_type_parameter aParameters[]; // size of 'parametersCount'
};

ANN_DEFINE(ketl_type_function) {
    KETL_TYPE_BODY;
    ketl_type_signature* pTypeSignature;
};


#endif // ketl_type_impl_h
