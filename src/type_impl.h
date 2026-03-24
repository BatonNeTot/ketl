//🫖ketl
#ifndef ketl_type_impl_h
#define ketl_type_impl_h

#include "ketl/type.h"

#include "variable.h"
#include "namespace.h"

#include "atomic_strings.h"

enum {
    KETL_TYPE_PRIMITIVE,
    KETL_TYPE_ENUM,
    KETL_TYPE_FUNCTION,
    KETL_TYPE_CFUNCTION,
    KETL_TYPE_ARRAY,
    KETL_TYPE_CLASS,
};

enum {
    KETL_ALIGN_1,
    KETL_ALIGN_2,
    KETL_ALIGN_4,
    KETL_ALIGN_8,
    KETL_ALIGN_16,
};

extern uint8_t ketl_align_map[5];

#define KETL_TYPE_BODY \
uint8_t kind;\
uint8_t align_enum;\
uint16_t size

ANN_DEFINE(ketl_type) {
    KETL_TYPE_BODY;
};

ANN_DEFINE(ketl_type_primitive) {
    KETL_TYPE_BODY;
    bool is_integer;
    bool is_signed;
    bool is_numeric;
};

ANN_DEFINE(ketl_type_array) {
    KETL_TYPE_BODY;
    ketl_type* p_value_type;
};

ANN_DEFINE(ketl_array) {
    uint8_t* p_data;
    uint64_t size;
    uint64_t capacity;
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
    ketl_symboled_variable_type_info_t* p_fields;
    ketl_namespace namespace;
};

ANN_DEFINE(ketl_type_enum_pair) {
    ketl_variable literal;
    ketl_atomic_string s_name;
};

ANN_DEFINE(ketl_type_enum) {
    KETL_TYPE_BODY;
    ketl_type_primitive* p_parent_primitive;
    uint64_t constants_count;
    ketl_type_enum_pair* p_contants;
    ketl_atomic_string s_name;
};

ANN_DEFINE(ketl_type_size_pair_t) {
    uint8_t align;
    uint16_t size;
};

ANN_FORWARD(ketl_state);

// align must be power of 2
uint8_t ketl_align_find(uint8_t align);

uint16_t ketl_type_get_stack_size(ketl_type* p_type);

uint8_t ketl_type_get_stack_align(ketl_type* p_type);

ketl_type_size_pair_t ketl_type_calc_class_size(ketl_symboled_variable_type_info_t* p_fields, uint16_t fields_count);

ketl_type* ketl_type_find_field_type(ketl_type* p_type, ketl_atomic_string s_name, ketl_state* p_state);

uint16_t ketl_type_get_field_offset(ketl_type* p_type, ketl_atomic_string s_name, ketl_state* p_state);

ketl_variable ketl_type_find_enum_constant_value(ketl_type* p_type, ketl_atomic_string s_name);

bool ketl_type_is_pointer_type(ketl_type* p_type);

bool ketl_type_is_raw_type(ketl_type* p_type);


#endif // ketl_type_impl_h
