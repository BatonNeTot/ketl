//🫖ketl
#ifndef ketl_type_impl_h
#define ketl_type_impl_h

#include "ketl/type.h"

#include "variable.h"

#include "atomic_strings.h"

enum {
    KETL_TYPE_PRIMITIVE,
    KETL_TYPE_ARRAY,
    KETL_TYPE_FUNCTION,
    KETL_TYPE_CFUNCTION,
    KETL_TYPE_CLASS,
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
    ketl_atomic_string s_name;
    bool is_integer;
    bool is_signed;
};

ANN_DEFINE(ketl_type_array) {
    KETL_TYPE_BODY;
    ketl_type* p_value_type;
};

ANN_DEFINE(ketl_type_signature) {
    uint16_t parameters_count;
	//ketl_type* p_return_type; return type is first parameter for now
    ketl_variable_type_info_t a_parameters[]; // size of 'parameters_count'
};

ANN_DEFINE(ketl_type_function) {
    KETL_TYPE_BODY;
    ketl_type_signature* p_type_signature;
};

ANN_DEFINE(ketl_symboled_variable_type_info_t) {
    ketl_variable_type_info_t info;
    ketl_atomic_string s_name;
};

ANN_DEFINE(ketl_type_class) {
    KETL_TYPE_BODY;
    ketl_atomic_string s_name;
    uint16_t fields_count;
    uint16_t methods_count;
    ketl_symboled_variable_type_info_t* p_fields;
    ketl_variable* p_methods;
};

ANN_DEFINE(ketl_type_size_pair_t) {
    uint8_t align;
    uint16_t size;
};

uint16_t ketl_type_get_stack_size(ketl_type* p_type);

ketl_type_size_pair_t ketl_type_calc_class_size(ketl_symboled_variable_type_info_t* p_fields, uint16_t fields_count);

ketl_type* ketl_type_find_class_field_type(ketl_type* p_type, ketl_atomic_string s_name);

uint16_t ketl_type_get_class_field_offset(ketl_type* p_type, ketl_atomic_string s_name);


#endif // ketl_type_impl_h
