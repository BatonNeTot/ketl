//🫖ketl
#include "hir.h"

#include "ketl_impl.h"

#include <stdio.h>

KETL_VECTOR_DEFINITION(hir_builder_instrs_t, uint8_t)
KETL_VECTOR_DEFINITION(hir_builder_vars_t, ketl_hir_var_t)
KETL_VECTOR_DEFINITION(hir_builder_used_globals_t, ketl_hir_global_t)
KETL_VECTOR_DEFINITION(hir_builder_blocks_t, ketl_hir_instr_offset_t)
KETL_VECTOR_DEFINITION(hir_builder_used_types_t, ketl_type*)

KETL_HASH_MAP_DEFINITION(hir_builder_symbol_to_var_map_t, ketl_hir_symbol_offset_t, ketl_hir_var_id_t, KETL_HASH_DEFAULT, KETL_EQUAL_DEFAULT)
KETL_HASH_MAP_DEFINITION(hir_builder_type_to_used_type_map_t, ketl_type*, ketl_hir_used_type_index_t, KETL_HASH_DEFAULT, KETL_EQUAL_DEFAULT)

void ketl_hir_builder_init(ketl_hir_builder_t* p_hir_builder, const ketl_allocator* p_allocator) {
    *p_hir_builder = (ketl_hir_builder_t){0};
    p_hir_builder->p_allocator = p_allocator;
    
    hir_builder_instrs_t_init(&p_hir_builder->v_instrs, 16, p_allocator);
    hir_builder_vars_t_init(&p_hir_builder->v_vars, 16, p_allocator);
    hir_builder_used_globals_t_init(&p_hir_builder->v_used_globals, 16, p_allocator);
    hir_builder_blocks_t_init(&p_hir_builder->v_blocks, 4, p_allocator);
    hir_builder_used_types_t_init(&p_hir_builder->v_used_types, 4, p_allocator);
    ketl_atomic_strings_init(&p_hir_builder->symbols, p_allocator);

    hir_builder_symbol_to_var_map_t_init(&p_hir_builder->m_symbol_to_var, p_allocator);
    hir_builder_type_to_used_type_map_t_init(&p_hir_builder->m_type_to_used_type, p_allocator);

    hir_builder_blocks_t_push_back_copy(&p_hir_builder->v_blocks, 0);
}

void ketl_hir_builder_flush(ketl_hir_builder_t* p_hir_builder, ketl_hir_t* p_hir) {
    p_hir->p_allocator = p_hir_builder->p_allocator;

    p_hir->p_instrs = p_hir_builder->v_instrs.pData;
    p_hir->p_vars = p_hir_builder->v_vars.pData;
    p_hir->p_used_globals = p_hir_builder->v_used_globals.pData;
    p_hir->p_block_offsets = p_hir_builder->v_blocks.pData;
    p_hir->p_used_types = p_hir_builder->v_used_types.pData;

    p_hir->instrs_count = p_hir_builder->v_instrs.size;
    p_hir->vars_count = p_hir_builder->v_vars.size;
    p_hir->used_global_count = p_hir_builder->v_used_globals.size;
    p_hir->blocks_count = p_hir_builder->v_blocks.size;
    p_hir->used_types_count = p_hir_builder->v_used_types.size;

    p_hir->p_symbols = p_hir_builder->symbols.vStorage.pData;
    ketl_atomic_strings_map_deinit(&p_hir_builder->symbols.mStrMap);

    hir_builder_symbol_to_var_map_t_deinit(&p_hir_builder->m_symbol_to_var);
    hir_builder_type_to_used_type_map_t_deinit(&p_hir_builder->m_type_to_used_type);
}

ketl_hir_used_type_index_t ketl_hir_builder_get_used_type_index(ketl_hir_builder_t* p_hir_builder, ketl_type* p_type) {
    hir_builder_type_to_used_type_map_t_bucket* p_bucket = hir_builder_type_to_used_type_map_t_get_or_insert_copy(&p_hir_builder->m_type_to_used_type, p_type, (ketl_hir_used_type_index_t)-1);
    // if size did change, insert new used type
    if (p_bucket->value == (ketl_hir_used_type_index_t)-1) {
        p_bucket->value = p_hir_builder->v_used_types.size;
        hir_builder_used_types_t_push_back_copy(&p_hir_builder->v_used_types, p_bucket->key);
    }
    return p_bucket->value;
}

ketl_hir_var_id_t ketl_hir_builder_get_literal(ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t literal, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_map_t_bucket* p_bucket = hir_builder_symbol_to_var_map_t_get_or_insert_copy(&p_hir_builder->m_symbol_to_var, literal, (ketl_hir_var_id_t)-1);
    // if size did change, insert new used type
    if (p_bucket->value == (ketl_hir_var_id_t)-1) {
        p_bucket->value = p_hir_builder->v_vars.size;
        hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
            .name = p_bucket->key,
            .type = type,
            .uid = KETL_HIR_VAR_UID_LITERAL,
        });
    }
    KETL_ASSERT(p_hir_builder->v_vars.pData[p_bucket->value].type == type);
    return p_bucket->value;
}

ketl_hir_var_id_t ketl_hir_builder_get_var(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t name, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_map_t_bucket* p_bucket = hir_builder_symbol_to_var_map_t_get_or_insert_copy(&p_hir_builder->m_symbol_to_var, name, (ketl_hir_var_id_t)-1);
    // if size did change, insert new used type
    if (p_bucket->value != (ketl_hir_var_id_t)-1) {
        KETL_ASSERT(type == KETL_HIR_USED_TYPE_UNKHOWN || p_hir_builder->v_vars.pData[p_bucket->value].type == type);
        return p_bucket->value;
    }

    const char* p_symbol = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, name);

    ketl_atomic_string s_symbol = ketl_atomic_strings_get(&p_state->atomicStrings, p_symbol, KETL_NULL_TERMINATED_LENGTH_32);
    ketl_namespace_node* p_symbol_node = ketl_namespace_find(&p_state->globalNamespace, s_symbol);
    if (p_symbol_node != NULL) {
        KETL_FOREVER {
            if (p_symbol_node->variable.type != KETL_VARIABLE_TYPE) {
                ketl_hir_used_global_index_t global_index = p_hir_builder->v_used_globals.size;
                hir_builder_used_globals_t_push_back_copy(&p_hir_builder->v_used_globals, (ketl_hir_global_t){
                    .p_variable = &p_symbol_node->variable,
                    .name = name,
                });

                ketl_hir_used_type_index_t global_type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_symbol_node->variable.pType);
                KETL_ASSERT(type == KETL_HIR_USED_TYPE_UNKHOWN || type == global_type);

                p_bucket->value = p_hir_builder->v_vars.size;
                hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
                    .global_index = global_index,
                    .type = global_type,
                    .uid = KETL_HIR_VAR_UID_GLOBAL,
                });

                return p_bucket->value;
            }

            if (p_symbol_node->nextOffset == (uint32_t)(-1)) {
                break;
            }
            p_symbol_node = p_state->globalNamespace.vNodes.pData + p_symbol_node->nextOffset;
        }
    }

    KETL_ASSERT(false);
    // TODO error undefined variable
    return (ketl_hir_var_id_t)-1;
}

ketl_hir_var_id_t ketl_hir_builder_register_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t name, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_map_t_bucket* p_bucket = hir_builder_symbol_to_var_map_t_get_or_insert_copy(&p_hir_builder->m_symbol_to_var, name, (ketl_hir_var_id_t)-1);
    // if size did change, insert new used type
    if (p_bucket->value != (ketl_hir_var_id_t)-1) {
        KETL_ASSERT(false);
        // TODO error redifinition
    }

    p_bucket->value = p_hir_builder->v_vars.size;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
        .name = p_bucket->key,
        .type = type,
        .uid = 0,
    });

    return p_bucket->value;
}

ketl_hir_var_id_t ketl_hir_builder_create_temp_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_map_t_bucket* p_bucket = hir_builder_symbol_to_var_map_t_get_or_insert_copy(&p_hir_builder->m_symbol_to_var, KETL_HIR_VAR_NAME_TEMP, (ketl_hir_var_id_t)-1);
    // if size did change, insert new used type
    if (p_bucket->value == (ketl_hir_var_id_t)-1) {
        p_bucket->value = p_hir_builder->v_vars.size;
        hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
            .name = p_bucket->key,
            .type = type,
            .uid = 0u,
        });

        return p_bucket->value;
    }

    ketl_hir_var_id_t temp_var = p_hir_builder->v_vars.size;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
        .name = p_bucket->key,
        .type = type,
        .uid = p_hir_builder->v_vars.pData[p_bucket->value].uid + 1,
    });

    p_bucket->value = temp_var;
    return temp_var;
}

void ketl_hir_builder_insert_instr(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, uint8_t* p_instr) {
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->v_instrs, (uint8_t*)&hir_header, sizeof(ketl_hir_header_t));
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->v_instrs, p_instr, ketl_hir_get_instr_size(hir_header.tag, p_instr));
}

void ketl_hir_builder_insert_binary_op(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_binary_op_t* p_binary_op) {
    KETL_SWITCH_STRICT (hir_header.tag) {
        case KETL_HIR_PLUS_UNDEF:
        case KETL_HIR_MINUS_UNDEF:
        case KETL_HIR_MULTY_UNDEF:
        case KETL_HIR_DIV_UNDEF: {
            // TODO FIX
            // for now we just hash search exact function, later we should take into acount possible implicit casts

            // first type is return type, ignored during search
            ketl_type_parameter parametersArray[] = { {.pType = NULL}, 
                {.pType = p_hir_builder->v_used_types.pData[p_hir_builder->v_vars.pData[p_binary_op->lhs_var].type]}, 
                {.pType = p_hir_builder->v_used_types.pData[p_hir_builder->v_vars.pData[p_binary_op->rhs_var].type]} };
            ketl_function_parameters parameters = {
                .pParameters = parametersArray,
                .parametersCount = sizeof(parametersArray) / sizeof(*parametersArray)
            };

            operator_overloading_map_bucket* p_operator_bucket = operator_overloading_map_get_or_null(p_state->amHIROperatorOverloading + (hir_header.tag - KETL_HIR_FIRST_UNDEF_OPERATOR), parameters);
            if (p_operator_bucket == NULL) {
                KETL_ASSERT(false); // TODO ERROR
            }
            
            // TODO FIX
            // check if return argument already has defined type and do casting if necessary
            p_hir_builder->v_vars.pData[p_binary_op->output_var].type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_operator_bucket->key.pParameters[0].pType);
            hir_header.tag = p_operator_bucket->value;
            break;
        }
    }

    ketl_hir_builder_insert_instr(p_hir_builder, hir_header, (uint8_t*)p_binary_op);
}

void ketl_hir_builder_insert_call(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_call_t* p_call, ketl_hir_var_id_t* p_arguments) {
    (void)p_state;
    // TODO do function determination
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->v_instrs, (uint8_t*)&hir_header, sizeof(ketl_hir_header_t));
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->v_instrs, (uint8_t*)p_call, sizeof(ketl_hir_call_t));
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->v_instrs, (uint8_t*)p_arguments, p_call->arguments_count * sizeof(*p_arguments));
}
