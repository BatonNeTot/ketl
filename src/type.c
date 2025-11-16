//🫖ketl
#include "type_impl.h"

size_t ketl_type_get_size(ketl_type* p_type) {
    return p_type->size;
}

uint16_t ketl_type_get_stack_size(ketl_type* p_type) {
    return p_type->type == KETL_TYPE_PRIMITIVE ? p_type->size : sizeof(void*);
}

size_t ketl_type_get_align(ketl_type* p_type) {
    return p_type->align;
}

ketl_type_size_pair_t ketl_type_calc_class_size(ketl_class_field_impl* p_fields, uint16_t fields_count) {
    ketl_type_size_pair_t pair = {
        .align = 1,
        .size = 0,
    };

    for (uint16_t i = 0u; i < fields_count; ++i) {
        uint8_t align = (uint8_t)ketl_type_get_align(p_fields[i].p_type.p_type);
        uint16_t size = (uint16_t)ketl_type_get_size(p_fields[i].p_type.p_type);

        if (align > pair.align) {
            pair.align = align;
        }

        pair.size = ANN_ALIGN_FORWARD(pair.size, align) + size;
    }

    pair.size = ANN_ALIGN_FORWARD(pair.size, pair.align);

    return pair;
}

ketl_type* ketl_type_find_class_field_type(ketl_type* p_type, ketl_atomic_string s_name) {
    uint16_t fields_count = ((ketl_type_class*)p_type)->fields_count;
    ketl_class_field_impl* p_fields = ((ketl_type_class*)p_type)->p_fields;

    for (uint16_t i = 0u; i < fields_count; ++i) {
        if (p_fields[i].s_name == s_name) {
            return p_fields[i].p_type.p_type;
        }
    }

    return NULL;
}

uint16_t ketl_type_get_class_field_offset(ketl_type* p_type, ketl_atomic_string s_name) {
    uint16_t fields_count = ((ketl_type_class*)p_type)->fields_count;
    ketl_class_field_impl* p_fields = ((ketl_type_class*)p_type)->p_fields;

    uint16_t offset = 0;

    for (uint16_t i = 0u; i < fields_count; ++i) {
        if (p_fields[i].s_name == s_name) {
            return offset;
        }

        uint8_t align = (uint8_t)ketl_type_get_align(p_fields[i].p_type.p_type);
        uint16_t size = (uint16_t)ketl_type_get_size(p_fields[i].p_type.p_type);

        offset = ANN_ALIGN_FORWARD(offset, align) + size;
    }

    return 0;
}
