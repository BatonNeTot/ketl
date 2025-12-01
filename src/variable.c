//🫖ketl
#include "variable.h"

#include "type_impl.h"


void ketl_variable_set_type(ketl_variable* p_variable, ketl_type* p_type) {
    p_variable->p_type = p_type;
    ANN_SWITCH_STRICT (p_type->type) {
        case KETL_TYPE_PRIMITIVE: {
            ketl_type_primitive* p_primitive_type = (ketl_type_primitive*)p_type;
            if (p_primitive_type->is_integer) {
                ANN_SWITCH_STRICT (p_primitive_type->size) {
                    case 1:
                        p_variable->type = p_primitive_type->is_signed ? KETL_VARIABLE_INT8 : KETL_VARIABLE_UINT8;
                        break;
                    case 2:
                        p_variable->type = p_primitive_type->is_signed ? KETL_VARIABLE_INT16 : KETL_VARIABLE_UINT16;
                        break;
                    case 4:
                        p_variable->type = p_primitive_type->is_signed ? KETL_VARIABLE_INT32 : KETL_VARIABLE_UINT32;
                        break;
                    case 8:
                        p_variable->type = p_primitive_type->is_signed ? KETL_VARIABLE_INT64 : KETL_VARIABLE_UINT64;
                        break;
                }
            } else {
                ANN_SWITCH_STRICT (p_primitive_type->size) {
                    case 0:
                        p_variable->type = KETL_VARIABLE_NONE;
                        break;
                    case 1:
                        p_variable->type = KETL_VARIABLE_BOOL;
                        break;
                    case 4:
                        p_variable->type = KETL_VARIABLE_FLOAT32;
                        break;
                    case 8:
                        p_variable->type = KETL_VARIABLE_FLOAT64;
                        break;
                }
            }
            break;
        }
        case KETL_TYPE_ARRAY: {
            p_variable->type = KETL_VARIABLE_POINTER;
            break;
        }
    }
}
