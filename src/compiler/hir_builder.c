//🫖ketl
#include "hir_builder.h"

#include "ketl_impl.h"
#include "error_stream.h"
#include "str.h"

#include <stdio.h>

KETL_VECTOR_DEFINITION(hir_builder_instrs_t, uint8_t)
KETL_VECTOR_DEFINITION(hir_builder_vars_t, ketl_hir_var_t)
KETL_VECTOR_DEFINITION(hir_builder_vars_infos_t, ketl_hir_var_info_t)
KETL_VECTOR_DEFINITION(hir_builder_blocks_t, ketl_hir_instr_offset_t)
KETL_VECTOR_DEFINITION(hir_builder_used_types_t, ketl_type*)

KETL_HASH_MAP_DEFINITION(hir_builder_symbol_to_var_info_map_t, ketl_hir_symbol_offset_t, ketl_hir_var_info_index_t, ANN_HASH, ANN_EQUAL)
KETL_HASH_MAP_DEFINITION(hir_builder_type_to_used_type_map_t, ketl_type*, ketl_hir_used_type_index_t, ANN_HASH, ANN_EQUAL)
KETL_HASH_MAP_DEFINITION(hir_builder_offset_to_block_t, ketl_hir_instr_offset_t, ketl_hir_block_index_t, ANN_HASH, ANN_EQUAL)

KETL_VECTOR_DEFINITION(hir_builder_return_offsets_t, ketl_hir_instr_offset_t)

KETL_VECTOR_DEFINITION(hir_builder_blocks_infos_t, hir_builder_block_info_t)


#define errorf(__offset, __length, ...) \
do {\
    ketl_error_info error_info = {\
        .p_lexer = p_hir_builder->p_lexer,\
        .s_filename = p_hir_builder->p_lexer->s_filename,\
        .offset = (__offset),\
        .length = (__length),\
    };\
    ketl_error_report(p_hir_builder->p_state, &error_info, __VA_ARGS__);\
} while (0)

#define GET_VAR(var_id) (p_hir_builder->vars.p_data[(var_id)])
#define GET_TYPE(used_type_index) (p_hir_builder->used_types.p_data[(used_type_index)])

static ketl_hir_var_info_t* get_var_info(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_info_index_t var_info_index) {
    return var_info_index != KETL_HIR_VAR_INFO_TEMP ? &p_hir_builder->vars_infos.p_data[var_info_index] : NULL;
} 

void ketl_hir_builder_init(ketl_hir_builder_t* p_hir_builder, ketl_state* p_state, ketl_lexer_t* p_lexer, const ketl_allocator* p_allocator) {
    *p_hir_builder = (ketl_hir_builder_t){0};
    p_hir_builder->p_allocator = p_allocator;
    p_hir_builder->p_state = p_state;
    p_hir_builder->p_lexer = p_lexer;
    
    hir_builder_instrs_t_init(&p_hir_builder->instrs, 16, p_allocator);
    hir_builder_vars_t_init(&p_hir_builder->vars, 16, p_allocator);
    hir_builder_vars_infos_t_init(&p_hir_builder->vars_infos, 16, p_allocator);
    hir_builder_blocks_t_init(&p_hir_builder->blocks, 4, p_allocator);
    hir_builder_used_types_t_init(&p_hir_builder->used_types, 4, p_allocator);
    ketl_atomic_strings_init(&p_hir_builder->symbols, p_allocator);

    hir_builder_return_offsets_t_init(&p_hir_builder->return_offsets, 4, p_allocator);
    hir_builder_blocks_infos_t_init(&p_hir_builder->blocks_infos, 4, p_allocator);

    hir_builder_symbol_to_var_info_map_t_init(&p_hir_builder->symbol_to_var, p_allocator);
    hir_builder_type_to_used_type_map_t_init(&p_hir_builder->type_to_used_type, p_allocator);
    hir_builder_offset_to_block_t_init(&p_hir_builder->offset_to_block, p_allocator);

    /////

    hir_builder_blocks_t_push_back_copy(&p_hir_builder->blocks, 0);
    hir_builder_offset_to_block_t_get_or_insert_copy(&p_hir_builder->offset_to_block, 0, 0);
}

void ketl_hir_builder_flush(ketl_hir_builder_t* p_hir_builder, ketl_hir_t* p_hir) {
    ANN_ASSERT(p_hir_builder->return_offsets.size > 0);
    ketl_hir_header_t* p_return = (ketl_hir_header_t*)(p_hir_builder->instrs.p_data + p_hir_builder->return_offsets.p_data[0]);
    
    if (p_return->tag == KETL_HIR_RETURN) {
        ketl_type* p_none_type = ketl_state_get_none_type(p_hir_builder->p_state);
        p_hir->return_type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_none_type);
    } else if ((p_return->tag & KETL_HIR_TYPE_INSTR_MASK) == KETL_HIR_RETURN_VALUE) {
        ketl_hir_return_value_t* p_return_info = (ketl_hir_return_value_t*)(((uint8_t*)p_return) + sizeof(ketl_hir_header_t));
        ketl_hir_var_t* p_return_var = &GET_VAR(p_return_info->value_var);
        p_hir->return_type = p_return_var->type;
    } else {
        ANN_ASSERT(false);
    }
    
    ////////////
    
    p_hir->p_allocator = p_hir_builder->p_allocator;
    
    p_hir->p_instrs = p_hir_builder->instrs.p_data;
    p_hir->p_vars = p_hir_builder->vars.p_data;
    p_hir->p_vars_infos = p_hir_builder->vars_infos.p_data;
    p_hir->p_block_offsets = p_hir_builder->blocks.p_data;
    p_hir->p_used_types = p_hir_builder->used_types.p_data;
    
    p_hir->instrs_count = p_hir_builder->instrs.size;
    p_hir->vars_count = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    p_hir->vars_infos_count = (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size;
    p_hir->blocks_count = (ketl_hir_block_index_t)p_hir_builder->blocks.size;
    p_hir->used_types_count = (ketl_hir_used_type_index_t)p_hir_builder->used_types.size;
    
    p_hir->p_symbols = p_hir_builder->symbols.storage.p_data;
    ketl_atomic_strings_map_deinit(&p_hir_builder->symbols.str_map);
    
    p_hir->parameter_count = p_hir_builder->parameter_count; 
    p_hir->has_calls = p_hir_builder->has_calls;
    
    ////////////////////////
    
    hir_builder_offset_to_block_t_deinit(&p_hir_builder->offset_to_block);
    hir_builder_blocks_infos_t_deinit(&p_hir_builder->blocks_infos);
    hir_builder_return_offsets_t_deinit(&p_hir_builder->return_offsets);
    
    hir_builder_symbol_to_var_info_map_t_deinit(&p_hir_builder->symbol_to_var);
    hir_builder_type_to_used_type_map_t_deinit(&p_hir_builder->type_to_used_type);
}

static void on_instr_inserted(ketl_hir_builder_t* p_hir_builder, ketl_hir_instr_offset_t instr_offset, ketl_hir_header_t hir_header) {
    if (hir_header.tag == KETL_HIR_RETURN) {
        hir_builder_return_offsets_t_push_back_copy(&p_hir_builder->return_offsets, instr_offset);
    } else if ((hir_header.tag & KETL_HIR_TYPE_INSTR_MASK) == KETL_HIR_RETURN_VALUE) {
        ketl_hir_return_value_t* p_return_info = (ketl_hir_return_value_t*)(p_hir_builder->instrs.p_data + instr_offset + sizeof(ketl_hir_header_t));
        ketl_hir_var_t* p_return_var = &GET_VAR(p_return_info->value_var);
        ANN_ASSERT(p_return_var->type != KETL_HIR_USED_TYPE_UNKNOWN);
        hir_builder_return_offsets_t_push_back_copy(&p_hir_builder->return_offsets, instr_offset);
    } else if (hir_header.tag == KETL_HIR_CALL || hir_header.tag == KETL_HIR_CALL_VOID) {
        p_hir_builder->has_calls = true;
    }
}

ketl_hir_used_type_index_t ketl_hir_builder_get_used_type_index(ketl_hir_builder_t* p_hir_builder, ketl_type* p_type) {
    if (p_type == NULL) {
        return KETL_HIR_USED_TYPE_UNKNOWN;
    }
    
    hir_builder_type_to_used_type_map_t_bucket* p_bucket = hir_builder_type_to_used_type_map_t_get_or_insert_copy(&p_hir_builder->type_to_used_type, p_type, (ketl_hir_used_type_index_t)-1);
    // if size did change, insert new used type
    if (p_bucket->value == (ketl_hir_used_type_index_t)-1) {
        p_bucket->value = (ketl_hir_used_type_index_t)p_hir_builder->used_types.size;
        hir_builder_used_types_t_push_back_copy(&p_hir_builder->used_types, p_bucket->key);
    }
    return p_bucket->value;
}

void ketl_hir_builder_add_parameter(ketl_hir_builder_t* p_hir_builder, ketl_namespace* p_namespace, ketl_named_variable_type_info_t* p_parameter_info) {
    ketl_atomic_string s_namespace_name = p_namespace->s_fullname;

    ketl_hir_symbol_offset_t name = ketl_atomic_strings_get(&p_hir_builder->symbols, p_parameter_info->p_name, p_parameter_info->name_length);
    ketl_hir_symbol_offset_t fullname = name;

    {
        const char* p_namespace_name = ketl_atomic_strings_get_pointer(&p_hir_builder->p_state->atomic_strings, s_namespace_name);
        uint64_t namespace_name_length = ketl_strlen(p_namespace_name);

        uint64_t total_length = namespace_name_length + p_parameter_info->name_length + 1;

        char a_buffer[256] = {0};
        ANN_ASSERT(total_length <= ANN_ARRAY_SIZE(a_buffer));
        ketl_memcpy(a_buffer, p_namespace_name, namespace_name_length);
        a_buffer[namespace_name_length] = '.';
        ketl_memcpy(a_buffer + namespace_name_length + 1, p_parameter_info->p_name, p_parameter_info->name_length);

        fullname = ketl_atomic_strings_get(&p_hir_builder->symbols, a_buffer, total_length);
    }
    
    hir_builder_symbol_to_var_info_map_t_bucket* p_bucket = hir_builder_symbol_to_var_info_map_t_get_or_insert_copy(&p_hir_builder->symbol_to_var, name, (ketl_hir_var_id_t)-1);
    // if size didn't change, var already exists
    if (p_bucket->value != (ketl_hir_var_id_t)-1) {
        ANN_ASSERT(false);
        // TODO error redifinition
    }
    p_bucket->value = (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size;
    hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
        .name = fullname,
    });
    
    ketl_hir_var_id_t var_id = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    get_var_info(p_hir_builder, p_bucket->value)->last_var_id = var_id;
    
    hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
        .info = p_bucket->value,
        .type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_parameter_info->info.p_type),
        .uid = KETL_HIR_VAR_UID_PARAMETER,
        .expr_info = { 
            .length = (ketl_hir_expr_length_t)-1,
            .source_offset = (ketl_token_offset_t)-1,
        },
    });

    ++p_hir_builder->parameter_count;
}

ketl_hir_var_id_t ketl_hir_builder_get_literal(ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t literal, ketl_hir_expr_info_t expr_info, ketl_hir_used_type_index_t type) {
    ketl_hir_var_id_t var_id = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
        .literal = literal,
        .type = type,
        .uid = KETL_HIR_VAR_UID_LITERAL,
        .expr_info = expr_info,
    });
    
    return var_id;
}

ketl_hir_var_id_t ketl_hir_builder_register_var(ketl_hir_builder_t* p_hir_builder, ketl_namespace* p_namespace, ketl_hir_symbol_offset_t name, ketl_hir_expr_info_t expr_info, ketl_hir_used_type_index_t type) {
    const char* p_var_name = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, name);
    uint64_t var_name_length = ketl_strlen(p_var_name);
    
    ketl_atomic_string s_namespace_name = p_namespace->s_fullname;

    ketl_hir_symbol_offset_t fullname = name;
    {
        const char* p_namespace_name = ketl_atomic_strings_get_pointer(&p_hir_builder->p_state->atomic_strings, s_namespace_name);
        uint64_t namespace_name_length = ketl_strlen(p_namespace_name);

        uint64_t total_length = namespace_name_length + var_name_length + 1;

        char a_buffer[256] = {0};
        ANN_ASSERT(total_length <= ANN_ARRAY_SIZE(a_buffer));
        ketl_memcpy(a_buffer, p_namespace_name, namespace_name_length);
        a_buffer[namespace_name_length] = '.';
        ketl_memcpy(a_buffer + namespace_name_length + 1, p_var_name, var_name_length);

        fullname = ketl_atomic_strings_get(&p_hir_builder->symbols, a_buffer, total_length);
        // symbols might had grown
        p_var_name = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, name);

        const char* p_symbol = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, fullname);
    
        ketl_atomic_string s_symbol = ketl_atomic_strings_get(&p_hir_builder->p_state->atomic_strings, p_symbol, KETL_NULL_TERMINATED_LENGTH_32);
        ketl_namespace_node* p_symbol_node = ketl_namespace_find(p_namespace, s_symbol);
        if (p_symbol_node != NULL) {
            ANN_ASSERT(false);
            // TODO error redifinition
        }
    }
    
    hir_builder_symbol_to_var_info_map_t_bucket* p_bucket = hir_builder_symbol_to_var_info_map_t_get_or_insert_copy(&p_hir_builder->symbol_to_var, name, (ketl_hir_var_id_t)-1);
    // if size didn't change, var already exists
    if (p_bucket->value != (ketl_hir_var_id_t)-1) {
        ANN_ASSERT(false);
        // TODO error redifinition
    }
    p_bucket->value = (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size;
    hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
        .name = fullname, // should I use here fullname?...
    });
    

    ketl_hir_var_id_t var_id = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    get_var_info(p_hir_builder, p_bucket->value)->last_var_id = var_id;

    hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
        .info = p_bucket->value,
        .type = type,
        .uid = 0u,
        .expr_info = expr_info,
    });

    return var_id;
}

ketl_hir_var_id_t ketl_hir_builder_get_var(ketl_hir_builder_t* p_hir_builder, ketl_namespace* p_namespace, ketl_hir_symbol_offset_t name, ketl_hir_expr_info_t expr_info, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_info_map_t_bucket* p_bucket = hir_builder_symbol_to_var_info_map_t_get_or_insert_copy(&p_hir_builder->symbol_to_var, name, (ketl_hir_var_id_t)-1);
    
    ketl_hir_used_type_index_t known_type;

    // if size didn't change, we found existing var
    if (p_bucket->value == (ketl_hir_var_id_t)-1) {
        const char* p_var_name = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, name);
        uint64_t var_name_length = ketl_strlen(p_var_name);
        
        ketl_atomic_string s_namespace_name = p_namespace->s_fullname;

        ketl_hir_symbol_offset_t fullname;
        {
            const char* p_namespace_name = ketl_atomic_strings_get_pointer(&p_hir_builder->p_state->atomic_strings, s_namespace_name);
            uint64_t namespace_name_length = ketl_strlen(p_namespace_name);

            uint64_t total_length = namespace_name_length + var_name_length + 1;

            char a_buffer[256] = {0};
            ANN_ASSERT(total_length <= ANN_ARRAY_SIZE(a_buffer));
            ketl_memcpy(a_buffer, p_namespace_name, namespace_name_length);
            a_buffer[namespace_name_length] = '.';
            ketl_memcpy(a_buffer + namespace_name_length + 1, p_var_name, var_name_length);

            fullname = ketl_atomic_strings_get(&p_hir_builder->symbols, a_buffer, total_length);
            // symbols might had grown
            p_var_name = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, name);
        }

        ketl_atomic_string s_symbol = ketl_atomic_strings_get(&p_hir_builder->p_state->atomic_strings, p_var_name, KETL_NULL_TERMINATED_LENGTH_32);
        ketl_namespace_node* p_symbol_node = ketl_namespace_find(p_namespace, s_symbol);

        if (p_symbol_node == NULL) {
            return ketl_hir_builder_create_temp_var(p_hir_builder, expr_info, KETL_HIR_USED_TYPE_UNKNOWN);
        }

        p_bucket->value = (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size;
        hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
            .name = fullname,
            .p_global = p_symbol_node,
        });

        if (p_symbol_node->variable.kind == KETL_VARIABLE_TYPE || p_symbol_node->variable.kind == KETL_VARIABLE_NAMESPACE) {
            known_type = KETL_HIR_USED_TYPE_META;
        } else {
            known_type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_symbol_node->variable.p_type);
        }

        ANN_ASSERT(type == KETL_HIR_USED_TYPE_UNKNOWN || type == known_type);

        ketl_hir_var_id_t var_id = (ketl_hir_var_id_t)p_hir_builder->vars.size;
        get_var_info(p_hir_builder, p_bucket->value)->last_var_id = var_id;

        hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
            .info = p_bucket->value,
            .type = known_type,
            .uid = KETL_HIR_VAR_UID_GLOBAL,
            .expr_info = expr_info,
        });

        return var_id;
    } 

    ketl_hir_var_t var = GET_VAR(get_var_info(p_hir_builder, p_bucket->value)->last_var_id);
    ANN_ASSERT(type == KETL_HIR_USED_TYPE_UNKNOWN || type == var.type);

    ketl_hir_var_id_t var_id = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    get_var_info(p_hir_builder, p_bucket->value)->last_var_id = var_id;

    var.expr_info = expr_info;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, var);

    return var_id;
}

ketl_hir_var_id_t ketl_hir_builder_get_global_var(ketl_hir_builder_t* p_hir_builder, ketl_namespace_node* p_namespace_node, ketl_hir_symbol_offset_t name, ketl_hir_expr_info_t expr_info, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_info_map_t_bucket* p_bucket = hir_builder_symbol_to_var_info_map_t_get_or_insert_copy(&p_hir_builder->symbol_to_var, name, (ketl_hir_var_id_t)-1);
    // if size didn't change, we found existing var
    if (p_bucket->value == (ketl_hir_var_id_t)-1) {
        p_bucket->value = (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size;
        hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
            .name = name,
            .p_global = p_namespace_node,
        });
    }
    
    ketl_hir_var_id_t var_id = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    get_var_info(p_hir_builder, p_bucket->value)->last_var_id = var_id;

    hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
        .info = p_bucket->value,
        .type = type,
        .uid = KETL_HIR_VAR_UID_GLOBAL,
        .expr_info = expr_info,
    });

    return var_id;
}

ketl_hir_var_id_t ketl_hir_builder_create_index_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t array_id, ketl_hir_var_id_t arg_id, ketl_hir_expr_info_t expr_info) {
    ANN_ASSERT(GET_VAR(array_id).type != KETL_HIR_USED_TYPE_UNKNOWN);
    ketl_type* p_array_type = GET_TYPE(GET_VAR(array_id).type);
    ANN_ASSERT(p_array_type->kind == KETL_TYPE_ARRAY);

    ketl_type* p_value_type = ((ketl_type_array*)p_array_type)->p_value_type;

    ketl_hir_var_info_index_t var_info = (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size;
    hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
        .arg_id = arg_id,
        .parent_id = array_id,
    });

    ketl_hir_var_id_t value = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
        .info = var_info,
        .type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_value_type),
        .uid = KETL_HIR_VAR_UID_INDEX,
        .expr_info = expr_info,
    });

    return value;
}

ketl_hir_var_id_t ketl_hir_builder_create_field_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t object_id, ketl_hir_symbol_offset_t name, ketl_hir_expr_info_t expr_info, ketl_hir_used_type_index_t type) {
    ketl_type* p_field_type = NULL;
    
    if (GET_VAR(object_id).type != KETL_HIR_USED_TYPE_UNKNOWN) {
        ketl_type* p_object_type = GET_TYPE(GET_VAR(object_id).type);
        
        const char* p_field_name = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, name);

        ketl_atomic_string s_field_name = ketl_atomic_strings_get(&p_hir_builder->p_state->atomic_strings, p_field_name, KETL_NULL_TERMINATED_LENGTH_32);
        p_field_type = ketl_type_find_class_field_type(p_object_type, s_field_name);

        if (p_field_type == NULL) {
            ANN_ASSERT(false);
            // TODO error unknown field
        }
    }

    ketl_hir_used_type_index_t field_type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_field_type);
    ANN_ASSERT(type == KETL_HIR_USED_TYPE_UNKNOWN || type == field_type);
    
    ketl_hir_var_info_index_t var_info = (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size;
    hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
        .name = name,
        .parent_id = object_id,
    });

    ketl_hir_var_id_t value = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
        .info = var_info,
        .type = field_type,
        .uid = KETL_HIR_VAR_UID_FIELD,
        .expr_info = expr_info,
    });

    return value;
}

ketl_hir_var_id_t ketl_hir_builder_create_temp_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_expr_info_t expr_info, ketl_hir_used_type_index_t type) {
    ketl_hir_var_id_t temp_var = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
        .info = KETL_HIR_VAR_INFO_TEMP,
        .type = type,
        .uid = p_hir_builder->temp_var_counter++,
        .expr_info = expr_info,
    });

    return temp_var;
}

void ketl_hir_builder_insert_instr(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, uint8_t* p_instr) {
    ketl_hir_instr_offset_t instr_offset = p_hir_builder->instrs.size;

    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, (uint8_t*)&hir_header, sizeof(ketl_hir_header_t));
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, p_instr, ketl_hir_get_instr_size(hir_header.tag, p_instr));

    on_instr_inserted(p_hir_builder, instr_offset, hir_header);
}

void ketl_hir_builder_insert_binary_op(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_binary_op_t* p_binary_op) {
    if ((hir_header.tag & KETL_HIR_TYPE_INSTR_MASK) && (hir_header.tag & KETL_HIR_TYPE_MASK) == KETL_HIR_UNDEF) {
        // TODO FIX
        // for now we just hash search exact function, later we should take into acount possible implicit casts

        // if any var is undefined, the op is undefined
        if (GET_VAR(p_binary_op->lhs_var).type != KETL_HIR_USED_TYPE_UNKNOWN &&
            GET_VAR(p_binary_op->rhs_var).type != KETL_HIR_USED_TYPE_UNKNOWN) {

            // first type is return type, ignored during search
            ketl_variable_type_info_t a_parameters_array[] = { {.p_type = NULL}, 
                {.p_type = GET_TYPE(GET_VAR(p_binary_op->lhs_var).type)}, 
                {.p_type = GET_TYPE(GET_VAR(p_binary_op->rhs_var).type)} };
            ketl_function_parameters parameters = {
                .p_parameters = a_parameters_array,
                .parameters_count = ANN_ARRAY_SIZE(a_parameters_array),
            };

            for (size_t i = 0; i < ANN_ARRAY_SIZE(a_parameters_array); ++i) {
                if (a_parameters_array[i].p_type && a_parameters_array[i].p_type->kind == KETL_TYPE_ENUM) {
                    a_parameters_array[i].p_type = (ketl_type*)((ketl_type_enum*)a_parameters_array[i].p_type)->p_parent_primitive;
                }
            }

            operator_overloading_map_bucket* p_operator_bucket = operator_overloading_map_get_or_null(
                p_hir_builder->p_state->am_hiroperator_overloading + ((hir_header.tag - KETL_HIR_FIRST_BI_OPERATOR) >> KETL_HIR_TYPE_INSTR_SHIFT), parameters);
            if (p_operator_bucket == NULL) {
                ANN_ASSERT(false); // TODO ERROR
            }
            
            // TODO FIX
            // check if return argument already has defined type and do casting if necessary
            GET_VAR(p_binary_op->output_var).type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_operator_bucket->key.p_parameters[0].p_type);
            hir_header.tag = p_operator_bucket->value;
        }
    }

    ketl_hir_builder_insert_instr(p_hir_builder, hir_header, (uint8_t*)p_binary_op);
}

void ketl_hir_builder_insert_call(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_call_t* p_call, ketl_hir_var_id_t* p_arguments) {
    ketl_hir_var_t* p_callee = &GET_VAR(p_call->callee);

    if (p_callee->type == KETL_HIR_USED_TYPE_UNKNOWN) {
        errorf(7, 14, "Trying to call an undefined entity.");
        return;
    }

    ketl_type* p_type = GET_TYPE(p_callee->type);

    if (p_type->kind != KETL_TYPE_FUNCTION && p_type->kind != KETL_TYPE_CFUNCTION) {
        // TODO error
        ANN_ASSERT(false);
    }
    ketl_type_function* p_function_type = (ketl_type_function*)p_type;
    ketl_type_signature* p_function_signature = p_function_type->p_type_signature;

    if (p_function_signature->parameters_count - 1 != p_call->arguments_count) {
        // TODO error
        ANN_ASSERT(false);
    }

    // TODO 
    // check argumnets types
    // do casting if needed
    // do template instantiation if needed
    
    ketl_hir_instr_offset_t instr_offset = p_hir_builder->instrs.size;

    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, (uint8_t*)&hir_header, sizeof(ketl_hir_header_t));
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, (uint8_t*)p_call, sizeof(ketl_hir_call_t));
    hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, (uint8_t*)p_arguments, p_call->arguments_count * sizeof(*p_arguments));

    on_instr_inserted(p_hir_builder, instr_offset, hir_header);
    
    // TODO FIX
    // check if return argument already has defined type and do casting if necessary
    ketl_hir_used_type_index_t return_type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_function_signature->a_parameters[0].p_type);
    GET_VAR(p_call->output_var).type = return_type;
}

void ketl_hir_builder_insert_new(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_new_t* p_create, ketl_hir_var_id_t* p_arguments) {
    ketl_namespace_node* p_type_node = get_var_info(p_hir_builder, GET_VAR(p_create->type_var).info)->p_global;
    ketl_type* p_type = p_type_node->variable.p_pointer;

    switch (p_type->kind) {
        case KETL_TYPE_ARRAY: {
            // TODO 
            // check argumnets types
            // do casting if needed
            // do template instantiation if needed

            ketl_hir_instr_offset_t instr_offset = p_hir_builder->instrs.size;

            hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, (uint8_t*)&hir_header, sizeof(ketl_hir_header_t));
            hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, (uint8_t*)p_create, sizeof(ketl_hir_call_t));
            hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, (uint8_t*)p_arguments, p_create->arguments_count * sizeof(*p_arguments));

            on_instr_inserted(p_hir_builder, instr_offset, hir_header);
            
            // TODO FIX
            // check if return argument already has defined type and do casting if necessary
            GET_VAR(p_create->output_var).type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_type);
            break;
        }
        case KETL_TYPE_CLASS: {
            // TODO find contructor
            ANN_ASSERT(p_create->arguments_count == 0);

            // TODO 
            // check argumnets types
            // do casting if needed
            // do template instantiation if needed

            ketl_hir_instr_offset_t instr_offset = p_hir_builder->instrs.size;

            hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, (uint8_t*)&hir_header, sizeof(ketl_hir_header_t));
            hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, (uint8_t*)p_create, sizeof(ketl_hir_call_t));
            hir_builder_instrs_t_push_back_ref_n(&p_hir_builder->instrs, (uint8_t*)p_arguments, p_create->arguments_count * sizeof(*p_arguments));

            on_instr_inserted(p_hir_builder, instr_offset, hir_header);
            
            // TODO FIX
            // check if return argument already has defined type and do casting if necessary
            GET_VAR(p_create->output_var).type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_type);
            
            break;
        }
        // TODO error
        default: ANN_ASSERT(false);
    }
}
