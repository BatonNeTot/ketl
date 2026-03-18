//🫖ketl
#include "type_impl.h"

#include "ketl_impl.h"

#include <stdio.h>

uint8_t ketl_align_map[5] = { 1, 2, 4, 8, 16 };

uint8_t ketl_align_find(uint8_t align) {
    uint8_t power = 0;
    while (align /= 2) {
        ++power;
    }
    return power;
}

size_t ketl_type_get_size(ketl_type* p_type) {
    return p_type ? p_type->size : 0;
}

uint16_t ketl_type_get_stack_size(ketl_type* p_type) {
    if (p_type == NULL) {
        return 0;
    }
    return p_type->kind == KETL_TYPE_PRIMITIVE ? p_type->size : sizeof(void*);
}

size_t ketl_type_get_align(ketl_type* p_type) {
    return ketl_align_map[ketl_type_get_size(p_type)];
}

bool ketl_type_is_array(ketl_type* p_type) {
    return p_type->kind == KETL_TYPE_ARRAY;
}

ketl_type_size_pair_t ketl_type_calc_class_size(ketl_symboled_variable_type_info_t* p_fields, uint16_t fields_count) {
    ketl_type_size_pair_t pair = {
        .align = 1,
        .size = 0,
    };

    for (uint16_t i = 0u; i < fields_count; ++i) {
        uint8_t align = (uint8_t)ketl_type_get_align(p_fields[i].info.p_type);
        uint16_t size = (uint16_t)ketl_type_get_size(p_fields[i].info.p_type);

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
    ketl_symboled_variable_type_info_t* p_fields = ((ketl_type_class*)p_type)->p_fields;

    for (uint16_t i = 0u; i < fields_count; ++i) {
        if (p_fields[i].s_name == s_name) {
            return p_fields[i].info.p_type;
        }
    }

    return NULL;
}

uint16_t ketl_type_get_class_field_offset(ketl_type* p_type, ketl_atomic_string s_name) {
    uint16_t fields_count = ((ketl_type_class*)p_type)->fields_count;
    ketl_symboled_variable_type_info_t* p_fields = ((ketl_type_class*)p_type)->p_fields;

    uint16_t offset = 0;

    for (uint16_t i = 0u; i < fields_count; ++i) {
        if (p_fields[i].s_name == s_name) {
            return offset;
        }

        uint8_t align = (uint8_t)ketl_type_get_align(p_fields[i].info.p_type);
        uint16_t size = (uint16_t)ketl_type_get_size(p_fields[i].info.p_type);

        offset = ANN_ALIGN_FORWARD(offset, align) + size;
    }

    return 0;
}

uint32_t ketl_type_format(ketl_state* p_state, ketl_type* p_type, char* p_buffer, uint32_t buffer_size) {
    ANN_SWITCH_STRICT(p_type->kind) {
        case KETL_TYPE_PRIMITIVE: {
            ketl_type_primitive* p_primitive_type = ((ketl_type_primitive*)p_type);
            if (p_primitive_type->is_integer) {
                ANN_SWITCH_STRICT(p_primitive_type->size) {
                    case 1: return p_primitive_type->is_signed ? 
                    snprintf(p_buffer, buffer_size, "i8") : snprintf(p_buffer, buffer_size, "u8");
                    case 2: return p_primitive_type->is_signed ? 
                    snprintf(p_buffer, buffer_size, "i16") : snprintf(p_buffer, buffer_size, "u16");
                    case 4: return p_primitive_type->is_signed ? 
                    snprintf(p_buffer, buffer_size, "i32") : snprintf(p_buffer, buffer_size, "u32");
                    case 8: return p_primitive_type->is_signed ? 
                    snprintf(p_buffer, buffer_size, "i64") : snprintf(p_buffer, buffer_size, "u64");
                }
            }
            return 0;
        }
        case KETL_TYPE_ARRAY: {
            uint32_t printed = ketl_type_format(p_state, ((ketl_type_array*)p_type)->p_value_type, p_buffer, buffer_size);
            return printed + snprintf(p_buffer + printed, buffer_size - printed, "[]");
        }
        case KETL_TYPE_CLASS: {
            return snprintf(p_buffer, buffer_size, "%s", 
                ketl_atomic_strings_get_pointer(&p_state->atomic_strings, ((ketl_type_class*)p_type)->s_name));
        }
    }
}
