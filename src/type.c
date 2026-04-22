//🫖ketl
#include "type_impl.h"

#include "ketl_impl.h"

#include "str.h"

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
    return ketl_type_is_pointer_type(p_type) ? sizeof(void*) : p_type->size;
}

size_t ketl_type_get_align(ketl_type* p_type) {
    return ketl_align_map[p_type ? p_type->align_enum : 0];
}

uint8_t ketl_type_get_stack_align(ketl_type* p_type) {
    uint8_t size = ketl_type_get_stack_size(p_type);
    ANN_ASSERT(size <= 8 && 8 == sizeof(void*));
    return size;
}

bool ketl_type_is_array(ketl_type* p_type) {
    return p_type->kind == KETL_TYPE_ARRAY;
}

static void ketl_type_calc_class_size_impl(ketl_type_size_pair_t* p_pair, ketl_symboled_variable_type_info_t* p_fields, uint16_t fields_count) {
    for (uint16_t i = 0u; i < fields_count; ++i) {
        if (p_fields[i].s_name == KETL_ATOMIC_STRING_EMPTY) {
            ketl_type* p_type = p_fields[i].info.p_type;
            ANN_ASSERT(p_type->kind == KETL_TYPE_CLASS);

            uint8_t align = (uint8_t)ketl_type_get_align(p_fields[i].info.p_type);
            uint16_t size = (uint16_t)ketl_type_get_size(p_fields[i].info.p_type);

            if (align > p_pair->align) {
                p_pair->align = align;
            }

            p_pair->size = ANN_ALIGN_FORWARD(p_pair->size, align) + size;
        } else {
            uint8_t align = (uint8_t)ketl_type_get_stack_align(p_fields[i].info.p_type);
            uint16_t size = (uint16_t)ketl_type_get_stack_size(p_fields[i].info.p_type);
            

            if (align > p_pair->align) {
                p_pair->align = align;
            }

            p_pair->size = ANN_ALIGN_FORWARD(p_pair->size, align) + size;
        }
    }
}

ketl_type_size_pair_t ketl_type_calc_class_size(ketl_symboled_variable_type_info_t* p_fields, uint16_t fields_count) {
    ketl_type_size_pair_t pair = {
        .align = 1,
        .size = 0,
    };

    ketl_type_calc_class_size_impl(&pair, p_fields, fields_count);

    pair.size = ANN_ALIGN_FORWARD(pair.size, pair.align);

    return pair;
}

ketl_type* ketl_type_find_field_type(ketl_type* p_type, ketl_atomic_string s_name, ketl_state* p_state) {
    if (p_type->kind == KETL_TYPE_CLASS) {
        uint16_t fields_count = ((ketl_type_class*)p_type)->fields_count;
        ketl_symboled_variable_type_info_t* p_fields = ((ketl_type_class*)p_type)->p_fields;

        for (uint16_t i = 0u; i < fields_count; ++i) {
            if (p_fields[i].s_name == KETL_ATOMIC_STRING_EMPTY) {
                ketl_type* p_extend_type = p_fields[i].info.p_type;
                ANN_ASSERT(p_extend_type->kind == KETL_TYPE_CLASS);
                ANN_ASSERT(p_type != p_extend_type);

                ketl_type* p_field_type = ketl_type_find_field_type(p_extend_type, s_name, p_state);
                if (p_field_type != NULL) {
                    return p_field_type;
                }
            } else if (p_fields[i].s_name == s_name) {
                return p_fields[i].info.p_type;
            }
        }
    } else if (p_type->kind == KETL_TYPE_ARRAY) {
        const char* p_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, s_name);

        if (ketl_str_is_equal("size", p_name)) {
            return ketl_state_get_u64(p_state);
        }
    }

    return NULL;
}

static bool ketl_type_get_class_field_offset(ketl_type_class* p_type, ketl_atomic_string s_name, uint16_t* p_offset) {
    ANN_ASSERT(p_type->kind == KETL_TYPE_CLASS);
    uint16_t fields_count = ((ketl_type_class*)p_type)->fields_count;
    ketl_symboled_variable_type_info_t* p_fields = ((ketl_type_class*)p_type)->p_fields;

    for (uint16_t i = 0u; i < fields_count; ++i) {
        if (p_fields[i].s_name == KETL_ATOMIC_STRING_EMPTY) {
            uint8_t align = (uint8_t)ketl_type_get_align(p_fields[i].info.p_type);
            uint16_t size = (uint16_t)ketl_type_get_size(p_fields[i].info.p_type);

            *p_offset = ANN_ALIGN_FORWARD(*p_offset, align);

            uint16_t offset;
            if (ketl_type_get_class_field_offset((ketl_type_class*)p_fields[i].info.p_type, s_name, &offset)) {
                *p_offset = offset;
                return true;
            }

            *p_offset += size;
        } else {
            uint8_t align = (uint8_t)ketl_type_get_stack_align(p_fields[i].info.p_type);
            uint16_t size = (uint16_t)ketl_type_get_stack_size(p_fields[i].info.p_type);

            *p_offset = ANN_ALIGN_FORWARD(*p_offset, align);
            
            if (p_fields[i].s_name == s_name) {
                return true;
            }

            *p_offset += size;
        }
    }

    return false;
}

uint16_t ketl_type_get_field_offset(ketl_type* p_type, ketl_atomic_string s_name, ketl_state* p_state) {
    if (p_type->kind == KETL_TYPE_CLASS) {
        uint16_t result = 0;
        if (ketl_type_get_class_field_offset((ketl_type_class*)p_type, s_name, &result)) {
            return result;
        }
    } else if (p_type->kind == KETL_TYPE_ARRAY) {
        const char* p_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, s_name);

        if (ketl_str_is_equal("size", p_name)) {
            return (uint16_t)offsetof(ketl_array, size);
        }
    }

    return 0;
}

uint16_t ketl_type_find_extended_class_offset(ketl_type* p_type, ketl_type* p_cast_target_type) {
    ANN_ASSERT(p_type->kind == KETL_TYPE_CLASS);
    uint16_t fields_count = ((ketl_type_class*)p_type)->fields_count;
    ketl_symboled_variable_type_info_t* p_fields = ((ketl_type_class*)p_type)->p_fields;

    uint16_t offset = 0;

    for (uint16_t i = 0u; i < fields_count; ++i) {
        if (p_fields[i].s_name == KETL_ATOMIC_STRING_EMPTY) {
            uint8_t align = (uint8_t)ketl_type_get_align(p_fields[i].info.p_type);
            uint16_t size = (uint16_t)ketl_type_get_size(p_fields[i].info.p_type);

            offset = ANN_ALIGN_FORWARD(offset, align);

            if (p_fields[i].info.p_type == p_cast_target_type) {
                return offset;
            }

            uint16_t inner_offset = ketl_type_find_extended_class_offset(p_fields[i].info.p_type, p_cast_target_type);
            if (inner_offset != (uint16_t)-1) {
                return offset + inner_offset;
            }

            offset += size;
        } else {
            uint8_t align = (uint8_t)ketl_type_get_stack_align(p_fields[i].info.p_type);
            uint16_t size = (uint16_t)ketl_type_get_stack_size(p_fields[i].info.p_type);

            offset = ANN_ALIGN_FORWARD(offset, align) + size;
        }
    }

    return (uint16_t)-1;
}

ketl_variable ketl_type_find_enum_constant_value(ketl_type* p_type, ketl_atomic_string s_name) {
    uint16_t constants_count = ((ketl_type_enum*)p_type)->constants_count;
    ketl_type_enum_pair* p_contants = ((ketl_type_enum*)p_type)->p_contants;

    for (uint16_t i = 0u; i < constants_count; ++i) {
        if (p_contants[i].s_name == s_name) {
            return p_contants[i].literal;
        }
    }

    return (ketl_variable){.kind = KETL_VARIABLE_NONE};
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

bool ketl_type_is_pointer_type(ketl_type* p_type) {
    return p_type && p_type->kind != KETL_TYPE_PRIMITIVE && p_type->kind != KETL_TYPE_ENUM;
}

bool ketl_type_is_raw_type(ketl_type* p_type) {
    return p_type && p_type->size == sizeof(void*) && p_type->kind == KETL_TYPE_PRIMITIVE && 
        !((ketl_type_primitive*)p_type)->is_integer && !((ketl_type_primitive*)p_type)->is_signed && !((ketl_type_primitive*)p_type)->is_numeric;
}

bool ketl_type_is_char_type(ketl_type* p_type) {
    return p_type && p_type->size == 1 && p_type->kind == KETL_TYPE_PRIMITIVE && 
        ((ketl_type_primitive*)p_type)->is_integer && !((ketl_type_primitive*)p_type)->is_signed && !((ketl_type_primitive*)p_type)->is_numeric;
}

bool ketl_type_is_u64_type(ketl_type* p_type) {
    return p_type && p_type->size == sizeof(void*) && p_type->kind == KETL_TYPE_PRIMITIVE && 
        ((ketl_type_primitive*)p_type)->is_integer && !((ketl_type_primitive*)p_type)->is_signed && ((ketl_type_primitive*)p_type)->is_numeric;
}
