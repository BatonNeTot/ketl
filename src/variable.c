//🫖ketl
#include "variable.h"

#include "type_impl.h"


void ketl_variable_set_type(ketl_variable* p_variable, ketl_type* p_type) {
    p_variable->p_type = p_type;
    p_variable->kind = ketl_variable_get_kind(p_type);
}

ketl_variable_kind ketl_variable_get_kind(ketl_type* p_type) {
    ANN_SWITCH_STRICT (p_type->kind) {
        case KETL_TYPE_PRIMITIVE: {
            ketl_type_primitive* p_primitive_type = (ketl_type_primitive*)p_type;
            if (p_primitive_type->is_integer) {
                ANN_SWITCH_STRICT (p_primitive_type->size) {
                    case 1:
                        return p_primitive_type->is_signed ? KETL_VARIABLE_INT8 : KETL_VARIABLE_UINT8;
                        break;
                    case 2:
                        return p_primitive_type->is_signed ? KETL_VARIABLE_INT16 : KETL_VARIABLE_UINT16;
                        break;
                    case 4:
                        return p_primitive_type->is_signed ? KETL_VARIABLE_INT32 : KETL_VARIABLE_UINT32;
                        break;
                    case 8:
                        return p_primitive_type->is_signed ? KETL_VARIABLE_INT64 : KETL_VARIABLE_UINT64;
                        break;
                }
            } else {
                ANN_SWITCH_STRICT (p_primitive_type->size) {
                    case 0:
                        return KETL_VARIABLE_NONE;
                        break;
                    case 1:
                        return KETL_VARIABLE_BOOL;
                        break;
                    case 4:
                        return KETL_VARIABLE_FLOAT32;
                        break;
                    case 8:
                        return KETL_VARIABLE_FLOAT64;
                        break;
                }
            }
            break;
        }
        case KETL_TYPE_ARRAY: {
            return KETL_VARIABLE_POINTER;
            break;
        }
        case KETL_TYPE_CLASS: {
            return KETL_VARIABLE_POINTER;
            break;
        }
        case KETL_TYPE_FUNCTION: {
            return KETL_VARIABLE_FUNC;
            break;
        }
        case KETL_TYPE_CFUNCTION: {
            return KETL_VARIABLE_CFUNC;
            break;
        }
    }
}
