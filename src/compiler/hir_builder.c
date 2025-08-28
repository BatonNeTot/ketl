//🫖ketl
#include "hir_builder.h"

#include "ketl_impl.h"

#include <stdio.h>

KETL_VECTOR_DEFINITION(hir_builder_instrs_t, uint8_t)
KETL_VECTOR_DEFINITION(hir_builder_vars_t, ketl_hir_var_t)
KETL_VECTOR_DEFINITION(hir_builder_vars_infos_t, ketl_hir_var_info_t)
KETL_VECTOR_DEFINITION(hir_builder_blocks_t, ketl_hir_instr_offset_t)
KETL_VECTOR_DEFINITION(hir_builder_used_types_t, ketl_type*)

KETL_HASH_MAP_DEFINITION(hir_builder_symbol_to_var_map_t, ketl_hir_symbol_offset_t, ketl_hir_var_id_t, ANN_HASH, ANN_EQUAL)
KETL_HASH_MAP_DEFINITION(hir_builder_type_to_used_type_map_t, ketl_type*, ketl_hir_used_type_index_t, ANN_HASH, ANN_EQUAL)
KETL_HASH_MAP_DEFINITION(hir_builder_offset_to_block_t, ketl_hir_instr_offset_t, ketl_hir_block_index_t, ANN_HASH, ANN_EQUAL)

KETL_VECTOR_DEFINITION(hir_builder_return_offsets_t, ketl_hir_instr_offset_t)

KETL_VECTOR_DEFINITION(hir_builder_blocks_infos_t, hir_builder_block_info_t)

void ketl_hir_builder_init(ketl_hir_builder_t* p_hir_builder, const ketl_allocator* p_allocator) {
    *p_hir_builder = (ketl_hir_builder_t){0};
    p_hir_builder->p_allocator = p_allocator;
    
    hir_builder_instrs_t_init(&p_hir_builder->v_instrs, 16, p_allocator);
    hir_builder_vars_t_init(&p_hir_builder->v_vars, 16, p_allocator);
    hir_builder_vars_infos_t_init(&p_hir_builder->v_vars_infos, 16, p_allocator);
    hir_builder_blocks_t_init(&p_hir_builder->v_blocks, 4, p_allocator);
    hir_builder_used_types_t_init(&p_hir_builder->v_used_types, 4, p_allocator);
    ketl_atomic_strings_init(&p_hir_builder->symbols, p_allocator);

    hir_builder_return_offsets_t_init(&p_hir_builder->v_return_offsets, 4, p_allocator);
    hir_builder_blocks_infos_t_init(&p_hir_builder->v_blocks_infos, 4, p_allocator);

    hir_builder_symbol_to_var_map_t_init(&p_hir_builder->m_symbol_to_var, p_allocator);
    hir_builder_type_to_used_type_map_t_init(&p_hir_builder->m_type_to_used_type, p_allocator);
    hir_builder_offset_to_block_t_init(&p_hir_builder->m_offset_to_block, p_allocator);

    /////

    hir_builder_blocks_t_push_back_copy(&p_hir_builder->v_blocks, 0);
    hir_builder_offset_to_block_t_get_or_insert_copy(&p_hir_builder->m_offset_to_block, 0, 0);
}

void ketl_hir_builder_flush(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_t* p_hir) {
    p_hir->p_allocator = p_hir_builder->p_allocator;

    p_hir->p_instrs = p_hir_builder->v_instrs.p_data;
    p_hir->p_vars = p_hir_builder->v_vars.p_data;
    p_hir->p_vars_infos = p_hir_builder->v_vars_infos.p_data;
    p_hir->p_block_offsets = p_hir_builder->v_blocks.p_data;
    p_hir->p_used_types = p_hir_builder->v_used_types.p_data;

    p_hir->instrs_count = p_hir_builder->v_instrs.size;
    p_hir->vars_count = (ketl_hir_var_id_t)p_hir_builder->v_vars.size;
    p_hir->vars_infos_count = (ketl_hir_var_info_index_t)p_hir_builder->v_vars_infos.size;
    p_hir->blocks_count = (ketl_hir_block_index_t)p_hir_builder->v_blocks.size;
    p_hir->used_types_count = (ketl_hir_used_type_index_t)p_hir_builder->v_used_types.size;

    p_hir->p_symbols = p_hir_builder->symbols.vStorage.p_data;
    ketl_atomic_strings_map_deinit(&p_hir_builder->symbols.mStrMap);

    (void)p_state;
    // ANN_ASSERT(p_hir_builder->v_return_offsets.size > 0);
    // uint8_t* p_return = p_hir->p_instrs + p_hir_builder->v_return_offsets.p_data[0];
    // if (((ketl_hir_header_t*)p_return)->tag == KETL_HIR_RETURN) {
    //     ketl_type* p_none_type = ketl_state_get_none_type(p_state);
    //     p_hir->return_type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_none_type);
    // } else if (((ketl_hir_header_t*)p_return)->tag == KETL_HIR_RETURN_VALUE) {
    //     ketl_hir_return_value_t* p_return_info = (ketl_hir_return_value_t*)(p_return + sizeof(ketl_hir_header_t));
    //     ketl_hir_var_t* p_return_var = &p_hir->p_vars[p_return_info->value_var];
    //     p_hir->return_type = p_return_var->type;
    // } else {
    //     ANN_ASSERT(false);
    // }

    hir_builder_offset_to_block_t_deinit(&p_hir_builder->m_offset_to_block);
    hir_builder_blocks_infos_t_deinit(&p_hir_builder->v_blocks_infos);
    hir_builder_return_offsets_t_deinit(&p_hir_builder->v_return_offsets);

    hir_builder_symbol_to_var_map_t_deinit(&p_hir_builder->m_symbol_to_var);
    hir_builder_type_to_used_type_map_t_deinit(&p_hir_builder->m_type_to_used_type);

}

static void on_instr_inserted(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header) {
    if (hir_header.tag == KETL_HIR_RETURN || hir_header.tag == KETL_HIR_RETURN_VALUE) {
        hir_builder_return_offsets_t_push_back_copy(&p_hir_builder->v_return_offsets, p_hir_builder->v_instrs.size);
    }
}

ketl_hir_used_type_index_t ketl_hir_builder_get_used_type_index(ketl_hir_builder_t* p_hir_builder, ketl_type* p_type) {
    hir_builder_type_to_used_type_map_t_bucket* p_bucket = hir_builder_type_to_used_type_map_t_get_or_insert_copy(&p_hir_builder->m_type_to_used_type, p_type, (ketl_hir_used_type_index_t)-1);
    // if size did change, insert new used type
    if (p_bucket->value == (ketl_hir_used_type_index_t)-1) {
        p_bucket->value = (ketl_hir_used_type_index_t)p_hir_builder->v_used_types.size;
        hir_builder_used_types_t_push_back_copy(&p_hir_builder->v_used_types, p_bucket->key);
    }
    return p_bucket->value;
}

ketl_hir_var_id_t ketl_hir_builder_get_literal(ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t literal, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_map_t_bucket* p_bucket = hir_builder_symbol_to_var_map_t_get_or_insert_copy(&p_hir_builder->m_symbol_to_var, literal, (ketl_hir_var_id_t)-1);
    // if size did change, insert new used type
    if (p_bucket->value == (ketl_hir_var_id_t)-1) {
        p_bucket->value = (ketl_hir_var_id_t)p_hir_builder->v_vars.size;
        hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
            .literal = p_bucket->key,
            .type = type,
            .uid = KETL_HIR_VAR_UID_LITERAL,
        });
    }
    ANN_ASSERT(p_hir_builder->v_vars.p_data[p_bucket->value].type == type);
    return p_bucket->value;
}

ketl_hir_var_id_t ketl_hir_builder_register_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t name, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_map_t_bucket* p_bucket = hir_builder_symbol_to_var_map_t_get_or_insert_copy(&p_hir_builder->m_symbol_to_var, name, (ketl_hir_var_id_t)-1);
    // if size didn't change, var already exists
    if (p_bucket->value != (ketl_hir_var_id_t)-1) {
        ANN_ASSERT(false);
        // TODO error redifinition
    }

    ketl_hir_var_info_index_t var_info = (ketl_hir_var_info_index_t)p_hir_builder->v_vars_infos.size;
    hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->v_vars_infos, (ketl_hir_var_info_t){
        .name = p_bucket->key,
        .p_global = NULL,
    });

    p_bucket->value = (ketl_hir_var_id_t)p_hir_builder->v_vars.size;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
        .info = var_info,
        .type = type,
        .uid = 0u,
    });

    return p_bucket->value;
}

ketl_hir_var_id_t ketl_hir_builder_get_var(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t name, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_map_t_bucket* p_bucket = hir_builder_symbol_to_var_map_t_get_or_insert_copy(&p_hir_builder->m_symbol_to_var, name, (ketl_hir_var_id_t)-1);
    // if size didn't change, we found existing var
    if (p_bucket->value != (ketl_hir_var_id_t)-1) {
        ANN_ASSERT(type == KETL_HIR_USED_TYPE_UNKNOWN || p_hir_builder->v_vars.p_data[p_bucket->value].type == type);
        return p_bucket->value;
    }

    const char* p_symbol = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, name);

    ketl_atomic_string s_symbol = ketl_atomic_strings_get(&p_state->atomicStrings, p_symbol, KETL_NULL_TERMINATED_LENGTH_32);
    ketl_namespace_node* p_symbol_node = ketl_namespace_find(&p_state->globalNamespace, s_symbol);
    if (p_symbol_node != NULL) {
        if (p_symbol_node->variable.type == KETL_VARIABLE_TYPE) {
            // TODO error
            ANN_ASSERT(false);
        }
        
        ketl_hir_var_info_index_t var_info = (ketl_hir_var_info_index_t)p_hir_builder->v_vars_infos.size;
        hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->v_vars_infos, (ketl_hir_var_info_t){
            .name = name,
            .p_global = &p_symbol_node->variable,
        });

        ketl_hir_used_type_index_t global_type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_symbol_node->variable.pType);
        ANN_ASSERT(type == KETL_HIR_USED_TYPE_UNKNOWN || type == global_type);

        p_bucket->value = (ketl_hir_var_id_t)p_hir_builder->v_vars.size;
        hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
            .info = var_info,
            .type = global_type,
            .uid = 0u,
        });

        return p_bucket->value;
    }

    return -1;
}

ketl_hir_var_id_t ketl_hir_builder_increment_var_uid(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t var_id) {
    ketl_hir_var_t* p_var = p_hir_builder->v_vars.p_data + var_id;
    ANN_ASSERT(p_var->uid != KETL_HIR_VAR_UID_LITERAL && p_var->info != KETL_HIR_VAR_INFO_TEMP);
    ketl_hir_var_info_t* p_var_info = p_hir_builder->v_vars_infos.p_data + p_var->info;
    ANN_ASSERT(p_var_info->p_global == NULL);

    hir_builder_symbol_to_var_map_t_bucket* p_bucket = hir_builder_symbol_to_var_map_t_get_or_null(&p_hir_builder->m_symbol_to_var, p_var_info->name);

    p_bucket->value = (ketl_hir_var_id_t)p_hir_builder->v_vars.size;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
        .info = p_var->info,
        .type = p_var->type,
        .uid = p_var->uid + 1,
    });

    return p_bucket->value;
}

ketl_hir_var_id_t ketl_hir_builder_create_temp_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_map_t_bucket* p_bucket = hir_builder_symbol_to_var_map_t_get_or_insert_copy(&p_hir_builder->m_symbol_to_var, KETL_HIR_VAR_NAME_TEMP, (ketl_hir_var_id_t)-1);
    // if size did change, insert new used type
    if (p_bucket->value == (ketl_hir_var_id_t)-1) {
        p_bucket->value = (ketl_hir_var_id_t)p_hir_builder->v_vars.size;
        hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
            .info = KETL_HIR_VAR_INFO_TEMP,
            .type = type,
            .uid = 0u,
        });

        return p_bucket->value;
    }

    ketl_hir_var_id_t temp_var = (ketl_hir_var_id_t)p_hir_builder->v_vars.size;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->v_vars, (ketl_hir_var_t){
        .info = KETL_HIR_VAR_INFO_TEMP,
        .type = type,
        .uid = p_hir_builder->v_vars.p_data[p_bucket->value].uid + 1,
    });

    p_bucket->value = temp_var;
    return temp_var;
}

void ketl_hir_builder_replace_temp_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t donor_var, ketl_hir_var_id_t temp_var) {
    ketl_hir_var_t* p_donor_var = p_hir_builder->v_vars.p_data + donor_var;
    ketl_hir_var_t* p_temp_var = p_hir_builder->v_vars.p_data + temp_var;
    ANN_ASSERT(p_donor_var->uid != KETL_HIR_VAR_UID_LITERAL && p_donor_var->info != KETL_HIR_VAR_INFO_TEMP);
    ANN_ASSERT(p_temp_var->uid != KETL_HIR_VAR_UID_LITERAL && p_temp_var->info == KETL_HIR_VAR_INFO_TEMP);
    ketl_hir_var_info_t* p_donor_info = p_hir_builder->v_vars_infos.p_data + p_donor_var->info;
    ANN_ASSERT(p_donor_info->p_global == NULL);

    // update symbol map for temp
    hir_builder_symbol_to_var_map_t_bucket* p_bucket = hir_builder_symbol_to_var_map_t_get_or_null(&p_hir_builder->m_symbol_to_var, KETL_HIR_VAR_NAME_TEMP);

    if (p_temp_var->uid == 0) {
        // remove temp var from map
        hir_builder_symbol_to_var_map_t_erase(&p_hir_builder->m_symbol_to_var, p_bucket);
    } else {
        // find prev temp var
        ketl_hir_var_uid_t old_uid = p_temp_var->uid - 1;
        ketl_hir_var_t* p_old_var = p_hir_builder->v_vars.p_data + (temp_var - 1);
        while (p_old_var->uid == KETL_HIR_VAR_UID_LITERAL || p_old_var->info != KETL_HIR_VAR_INFO_TEMP || p_old_var->uid != old_uid) {
            --p_old_var;
        }
        p_bucket->value = (ketl_hir_var_id_t)(p_old_var - p_hir_builder->v_vars.p_data);
    }

    // copy data from donor
    p_hir_builder->v_vars.p_data[temp_var] = p_hir_builder->v_vars.p_data[donor_var];

    // update symbol map for donor
    p_bucket = hir_builder_symbol_to_var_map_t_get_or_null(&p_hir_builder->m_symbol_to_var, p_donor_info->name);
    p_bucket->value = temp_var;

    // TDO fix later
    // donor_var is now leaking unused
}

void ketl_hir_builder_insert_instr(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, uint8_t* p_instr) {
    on_instr_inserted(p_hir_builder, hir_header);

    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->v_instrs, (uint8_t*)&hir_header, sizeof(ketl_hir_header_t));
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->v_instrs, p_instr, ketl_hir_get_instr_size(hir_header.tag, p_instr));
}

void ketl_hir_builder_insert_binary_op(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_binary_op_t* p_binary_op) {
    if (hir_header.tag >= KETL_HIR_FIRST_UNDEF_OPERATOR && hir_header.tag <= KETL_HIR_LAST_UNDEF_OPERATOR) {
        // TODO FIX
        // for now we just hash search exact function, later we should take into acount possible implicit casts

        // if any var is undefined, the op is undefined
        if (p_hir_builder->v_vars.p_data[p_binary_op->lhs_var].type != KETL_HIR_USED_TYPE_UNKNOWN &&
            p_hir_builder->v_vars.p_data[p_binary_op->rhs_var].type != KETL_HIR_USED_TYPE_UNKNOWN) {

            // first type is return type, ignored during search
            ketl_type_parameter parametersArray[] = { {.pType = NULL}, 
                {.pType = p_hir_builder->v_used_types.p_data[p_hir_builder->v_vars.p_data[p_binary_op->lhs_var].type]}, 
                {.pType = p_hir_builder->v_used_types.p_data[p_hir_builder->v_vars.p_data[p_binary_op->rhs_var].type]} };
            ketl_function_parameters parameters = {
                .pParameters = parametersArray,
                .parametersCount = sizeof(parametersArray) / sizeof(*parametersArray)
            };

            operator_overloading_map_bucket* p_operator_bucket = operator_overloading_map_get_or_null(p_state->amHIROperatorOverloading + (hir_header.tag - KETL_HIR_FIRST_UNDEF_OPERATOR), parameters);
            if (p_operator_bucket == NULL) {
                ANN_ASSERT(false); // TODO ERROR
            }
            
            // TODO FIX
            // check if return argument already has defined type and do casting if necessary
            p_hir_builder->v_vars.p_data[p_binary_op->output_var].type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_operator_bucket->key.pParameters[0].pType);
            hir_header.tag = p_operator_bucket->value;
        }
    }

    ketl_hir_builder_insert_instr(p_hir_builder, hir_header, (uint8_t*)p_binary_op);
}

void ketl_hir_builder_insert_call(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_call_t* p_call, ketl_hir_var_id_t* p_arguments) {
    // TODO perhaps var content should be enough
    (void)p_state;

    ketl_hir_var_t* p_callee = &p_hir_builder->v_vars.p_data[p_call->callee];
    ketl_type* p_type = p_hir_builder->v_used_types.p_data[p_callee->type];

    if (p_type->type != KETL_TYPE_CFUNCTION) {
        // TODO error
        ANN_ASSERT(false);
    }
    ketl_type_function* p_function_type = (ketl_type_function*)p_type;
    ketl_type_signature* p_function_signature = p_function_type->pTypeSignature;

    if (p_function_signature->parametersCount - 1 != p_call->arguments_count) {
        // TODO error
        ANN_ASSERT(false);
    }

    // TODO 
    // check argumnets types
    // do casting if needed
    // do template instantiation if needed

    on_instr_inserted(p_hir_builder, hir_header);

    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->v_instrs, (uint8_t*)&hir_header, sizeof(ketl_hir_header_t));
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->v_instrs, (uint8_t*)p_call, sizeof(ketl_hir_call_t));
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->v_instrs, (uint8_t*)p_arguments, p_call->arguments_count * sizeof(*p_arguments));
    
    // TODO FIX
    // check if return argument already has defined type and do casting if necessary
    p_hir_builder->v_vars.p_data[p_call->output_var].type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_function_signature->aParameters[0].pType);
}
