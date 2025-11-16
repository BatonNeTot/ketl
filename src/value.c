//🫖ketl
#include "value_impl.h"

#include "ketl_impl.h"

#include "type_impl.h" 


bool ketl_value_is_none(ketl_state* p_state, ketl_value* p_value) {
    return ketl_value_get_type(p_state, p_value) == ketl_state_get_none_type(p_state);
}

ketl_type* ketl_value_get_type(ketl_state* p_state, ketl_value* p_value) {
    (void)p_state;

    ketl_variable* p_variable = (ketl_variable*)p_value;
    return p_variable->p_type;
}

void* ketl_value_as_raw(ketl_state* p_state, ketl_value* p_value) {
    (void)p_state;

    ANN_ASSERT(ketl_value_get_type(p_state, p_value) == ketl_state_get_raw_type(p_state));
    ketl_variable* p_variable = (ketl_variable*)p_value;
    return p_variable->pointer;
}

int8_t ketl_value_as_i8(ketl_state* p_state, ketl_value* p_value) {
    (void)p_state;

    ANN_ASSERT(ketl_value_get_type(p_state, p_value) == ketl_state_get_i8(p_state));
    ketl_variable* p_variable = (ketl_variable*)p_value;
    return p_variable->int8;
}

int16_t ketl_value_as_i16(ketl_state* p_state, ketl_value* p_value) {
    (void)p_state;

    ANN_ASSERT(ketl_value_get_type(p_state, p_value) == ketl_state_get_i16(p_state));
    ketl_variable* p_variable = (ketl_variable*)p_value;
    return p_variable->int16;
}

int32_t ketl_value_as_i32(ketl_state* p_state, ketl_value* p_value) {
    (void)p_state;

    ANN_ASSERT(ketl_value_get_type(p_state, p_value) == ketl_state_get_i32(p_state));
    ketl_variable* p_variable = (ketl_variable*)p_value;
    return p_variable->int32;
}

int64_t ketl_value_as_i64(ketl_state* p_state, ketl_value* p_value) {
    (void)p_state;

    ANN_ASSERT(ketl_value_get_type(p_state, p_value) == ketl_state_get_i64(p_state));
    ketl_variable* p_variable = (ketl_variable*)p_value;
    return p_variable->int64;
}

void ketl_value_destroy(ketl_state* p_state, ketl_value* p_value) {
    ketl_free(p_state->p_allocator, p_value);
}


ketl_value* ketl_value_from_variable(ketl_variable variable, const ketl_allocator* p_allocator) {
    ketl_variable* p_variable = (ketl_variable*)ketl_alloc(p_allocator, sizeof(ketl_variable));
    *p_variable = variable;
    return (ketl_value*)p_variable;
}
