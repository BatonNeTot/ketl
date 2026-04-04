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
KETL_VECTOR_DEFINITION(ketl_hir_consts_infos_t, ketl_hir_const_info_t)
KETL_VECTOR_DEFINITION(ketl_hir_consts_t, uint8_t)

KETL_HASH_MAP_DEFINITION(hir_builder_symbol_to_var_info_map_t, ketl_hir_symbol_offset_t, ketl_hir_var_info_index_t, ANN_HASH, ANN_EQUAL)
KETL_HASH_MAP_DEFINITION(hir_builder_type_to_used_type_map_t, ketl_type*, ketl_hir_used_type_index_t, ANN_HASH, ANN_EQUAL)
KETL_HASH_MAP_DEFINITION(hir_builder_offset_to_block_t, ketl_hir_instr_offset_t, ketl_hir_block_index_t, ANN_HASH, ANN_EQUAL)

KETL_VECTOR_DEFINITION(hir_builder_symbol_stack_t, hir_builder_symbol_to_var_info_map_t)

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

#define EOF_STR "EOF"
#define TOKEN_LENGTH(token) ((int)((token).length > 0 ? (token).length : sizeof(EOF_STR) - 1))
#define TOKEN_STRING(token) ((token).length > 0 ? (p_hir_builder)->p_lexer->p_source + (token).offset : EOF_STR)

static ketl_hir_var_info_t* get_var_info(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_info_index_t var_info_index) {
    return var_info_index != KETL_HIR_VAR_INFO_TEMP ? &p_hir_builder->vars_infos.p_data[var_info_index] : NULL;
} 

void ketl_hir_builder_init(ketl_hir_builder_t* p_hir_builder, ketl_state* p_state, ketl_lexer_t* p_lexer, ketl_type* p_return_type, uint16_t func_index, const ketl_allocator* p_allocator) {
    *p_hir_builder = (ketl_hir_builder_t){0};
    p_hir_builder->p_allocator = p_allocator;
    p_hir_builder->p_state = p_state;
    p_hir_builder->p_lexer = p_lexer;
    p_hir_builder->p_return_type = p_return_type;
    p_hir_builder->max_call_arg_count = (uint8_t)-1;
    p_hir_builder->func_index = func_index;
    
    hir_builder_instrs_t_init(&p_hir_builder->instrs, 16, p_allocator);
    hir_builder_vars_t_init(&p_hir_builder->vars, 16, p_allocator);
    hir_builder_vars_infos_t_init(&p_hir_builder->vars_infos, 16, p_allocator);
    hir_builder_blocks_t_init(&p_hir_builder->blocks, 4, p_allocator);
    hir_builder_used_types_t_init(&p_hir_builder->used_types, 4, p_allocator);
    ketl_atomic_strings_init(&p_hir_builder->symbols, p_allocator);
    ketl_hir_consts_infos_t_init(&p_hir_builder->consts_infos, 4, p_allocator);
    ketl_hir_consts_t_init(&p_hir_builder->consts, 4, p_allocator);

    hir_builder_return_offsets_t_init(&p_hir_builder->return_offsets, 4, p_allocator);
    hir_builder_blocks_infos_t_init(&p_hir_builder->blocks_infos, 4, p_allocator);

    hir_builder_symbol_stack_t_init(&p_hir_builder->symbol_to_var_info_stack, 1, p_allocator);
    hir_builder_type_to_used_type_map_t_init(&p_hir_builder->type_to_used_type, p_allocator);
    hir_builder_offset_to_block_t_init(&p_hir_builder->offset_to_block, p_allocator);

    /////

    hir_builder_blocks_t_push_back_copy(&p_hir_builder->blocks, 0);
    hir_builder_offset_to_block_t_get_or_insert_copy(&p_hir_builder->offset_to_block, 0, 0);

    /////////

    hir_builder_symbol_stack_t_push_back_copy(&p_hir_builder->symbol_to_var_info_stack, (hir_builder_symbol_to_var_info_map_t){0});
    hir_builder_symbol_to_var_info_map_t_init(&p_hir_builder->symbol_to_var_info_stack.p_data[0], p_allocator);
}

void ketl_hir_builder_flush(ketl_hir_builder_t* p_hir_builder, ketl_hir_t* p_hir) {
    if (p_hir_builder->p_return_type == NULL) {
        ANN_ASSERT(p_hir_builder->return_offsets.size > 0);
        // TODO deduce return type instead of getting just first
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
    } else {
        p_hir->return_type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_hir_builder->p_return_type);
    }
    
    ////////////
    
    p_hir->p_allocator = p_hir_builder->p_allocator;
    
    p_hir->p_instrs = p_hir_builder->instrs.p_data;
    p_hir->p_vars = p_hir_builder->vars.p_data;
    p_hir->p_vars_infos = p_hir_builder->vars_infos.p_data;
    p_hir->p_block_offsets = p_hir_builder->blocks.p_data;
    p_hir->p_used_types = p_hir_builder->used_types.p_data;
    p_hir->p_consts_infos = p_hir_builder->consts_infos.p_data;
    p_hir->p_consts = p_hir_builder->consts.p_data;
    
    p_hir->instrs_count = p_hir_builder->instrs.size;
    p_hir->vars_count = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    p_hir->vars_infos_count = (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size;
    p_hir->blocks_count = (ketl_hir_block_index_t)p_hir_builder->blocks.size;
    p_hir->used_types_count = (ketl_hir_used_type_index_t)p_hir_builder->used_types.size;
    p_hir->consts_count = (ketl_hir_const_index_t)p_hir_builder->consts_infos.size;
    p_hir->consts_size = p_hir_builder->consts.size;
    
    p_hir->p_symbols = p_hir_builder->symbols.storage.p_data;
    ketl_atomic_strings_map_deinit(&p_hir_builder->symbols.str_map);
    
    p_hir->parameter_count = p_hir_builder->parameter_count; 
    p_hir->max_call_arg_count = p_hir_builder->max_call_arg_count;
    
    ////////////////////////

    ANN_ASSERT(p_hir_builder->symbol_to_var_info_stack.size == 1);
    hir_builder_symbol_to_var_info_map_t_deinit(&p_hir_builder->symbol_to_var_info_stack.p_data[0]);

    ////////////////////
    
    hir_builder_offset_to_block_t_deinit(&p_hir_builder->offset_to_block);
    hir_builder_blocks_infos_t_deinit(&p_hir_builder->blocks_infos);
    hir_builder_return_offsets_t_deinit(&p_hir_builder->return_offsets);
    
    hir_builder_symbol_stack_t_deinit(&p_hir_builder->symbol_to_var_info_stack);
    hir_builder_type_to_used_type_map_t_deinit(&p_hir_builder->type_to_used_type);
}

static void on_instr_inserted(ketl_hir_builder_t* p_hir_builder, ketl_hir_instr_offset_t instr_offset, ketl_hir_header_t hir_header) {
    if (hir_header.tag == KETL_HIR_RETURN) {
        hir_builder_return_offsets_t_push_back_copy(&p_hir_builder->return_offsets, instr_offset);
    } else if ((hir_header.tag & KETL_HIR_TYPE_INSTR_MASK) == KETL_HIR_RETURN_VALUE) {
        //ketl_hir_return_value_t* p_return_info = (ketl_hir_return_value_t*)(p_hir_builder->instrs.p_data + instr_offset + sizeof(ketl_hir_header_t));
        //ketl_hir_var_t* p_return_var = &GET_VAR(p_return_info->value_var);
        //ANN_ASSERT(p_return_var->type != KETL_HIR_USED_TYPE_UNKNOWN);
        hir_builder_return_offsets_t_push_back_copy(&p_hir_builder->return_offsets, instr_offset);
    } else {
        switch (hir_header.tag) {
            case KETL_HIR_CALL:
            case KETL_HIR_CALL_VOID: 

            case KETL_HIR_NEW:

            case KETL_HIR_CREATE_ARRAY:
            case KETL_HIR_CREATE_SLICE:
            case KETL_HIR_APPEND_VALUE: {
                uint8_t call_arg_count = 0;
                
                switch (hir_header.tag) {
                    case KETL_HIR_CALL: {
                        ketl_hir_call_t* p_call_info = (ketl_hir_call_t*)(p_hir_builder->instrs.p_data + instr_offset + sizeof(ketl_hir_header_t));
                        call_arg_count = p_call_info->arguments_count;
                        break;
                    } 
                    case KETL_HIR_CALL_VOID: {
                        ketl_hir_call_void_t* p_call_info = (ketl_hir_call_void_t*)(p_hir_builder->instrs.p_data + instr_offset + sizeof(ketl_hir_header_t));
                        call_arg_count = p_call_info->arguments_count;
                        break;
                    }
                    case KETL_HIR_NEW: {
                        ketl_hir_new_t* p_call_info = (ketl_hir_new_t*)(p_hir_builder->instrs.p_data + instr_offset + sizeof(ketl_hir_header_t));

                        if (p_hir_builder->max_call_arg_count == (uint8_t)-1 ||
                            p_hir_builder->max_call_arg_count < p_call_info->arguments_count) {
                            p_hir_builder->max_call_arg_count = p_call_info->arguments_count;
                        }

                        call_arg_count = 3; // gc call to create a class
                        break;
                    }
                    case KETL_HIR_CREATE_ARRAY: {
                        call_arg_count = 6; // gc call to create a array
                        break;
                    }
                    case KETL_HIR_CREATE_SLICE: {
                        call_arg_count = 6; // gc call to create a slice
                        break;
                    }
                    case KETL_HIR_APPEND_VALUE: {
                        call_arg_count = 4; // gc call to append value
                        break;
                    }
                }

                if (p_hir_builder->max_call_arg_count == (uint8_t)-1 ||
                    p_hir_builder->max_call_arg_count < call_arg_count) {
                    p_hir_builder->max_call_arg_count = call_arg_count;
                }
            }
        }
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

static hir_builder_symbol_to_var_info_map_t_bucket* find_info_by_symbol(ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t name) {
    for (uint32_t i = p_hir_builder->symbol_to_var_info_stack.size - 1; i != (uint32_t)-1; --i) {
        hir_builder_symbol_to_var_info_map_t_bucket* p_bucket = hir_builder_symbol_to_var_info_map_t_get_or_null(&p_hir_builder->symbol_to_var_info_stack.p_data[i], name);
        if (p_bucket != NULL) {
            return p_bucket;
        }
    }
    return NULL;
}

void ketl_hir_builder_add_parameter(ketl_hir_builder_t* p_hir_builder, ketl_namespace* p_namespace, ketl_named_variable_type_info_t* p_parameter_info) {
    //ketl_atomic_string s_namespace_name = p_namespace->s_fullname;
    (void)p_namespace;

    ketl_hir_symbol_offset_t name = ketl_atomic_strings_get(&p_hir_builder->symbols, p_parameter_info->p_name, p_parameter_info->name_length);
    //ketl_hir_symbol_offset_t fullname = name;

    /*
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
    */
    
    hir_builder_symbol_to_var_info_map_t_bucket* p_bucket = hir_builder_symbol_to_var_info_map_t_get_or_insert_copy(&p_hir_builder->symbol_to_var_info_stack.p_data[0], name, (ketl_hir_var_id_t)-1);
    // if size didn't change, var already exists
    if (p_bucket->value != (ketl_hir_var_id_t)-1) {
        ANN_ASSERT(false);
        // TODO error redifinition
    }
    p_bucket->value = (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size;
    hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
        .name = name,
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

        const char* p_symbol = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, fullname);
    
        ketl_atomic_string s_symbol = ketl_atomic_strings_get(&p_hir_builder->p_state->atomic_strings, p_symbol, KETL_NULL_TERMINATED_LENGTH_32);
        ketl_namespace_node* p_symbol_node = ketl_namespace_find(p_namespace, s_symbol);
        if (p_symbol_node != NULL) {
            ANN_ASSERT(false);
            // TODO error redifinition
        }

        // symbols might had grown
        p_var_name = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, name);
    }
    
    hir_builder_symbol_to_var_info_map_t_bucket* p_bucket = hir_builder_symbol_to_var_info_map_t_get_or_insert_copy(
        &p_hir_builder->symbol_to_var_info_stack.p_data[p_hir_builder->symbol_to_var_info_stack.size - 1],
        name, (ketl_hir_var_id_t)-1);
    // if size didn't change, var already exists
    if (p_bucket->value != (ketl_hir_var_id_t)-1) {
        errorf(expr_info.source_offset, expr_info.length, "Redefinition of the symbol '%s'", p_var_name);
        return ketl_hir_builder_create_temp_var(p_hir_builder, expr_info, KETL_HIR_USED_TYPE_UNKNOWN);
    }
    p_bucket->value = (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size;
    hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
        .name = name,
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

// TODO contruct fullname instead of just using expr_info cutted symbol
ketl_hir_var_id_t ketl_hir_builder_get_var(ketl_hir_builder_t* p_hir_builder, ketl_namespace* p_namespace, ketl_hir_symbol_offset_t name, ketl_hir_symbol_offset_t fullname, ketl_hir_expr_info_t expr_info, bool force) {
    hir_builder_symbol_to_var_info_map_t_bucket* p_bucket = find_info_by_symbol(p_hir_builder, fullname);
    
    ketl_hir_used_type_index_t known_type;

    // if size didn't change, we found existing var
    if (p_bucket == NULL) {
        const char* p_var_name = ketl_atomic_strings_get_pointer(&p_hir_builder->symbols, name);

        ketl_atomic_string s_symbol = ketl_atomic_strings_get(&p_hir_builder->p_state->atomic_strings, p_var_name, KETL_NULL_TERMINATED_LENGTH_32);
        ketl_namespace_node* p_symbol_node = ketl_namespace_find(p_namespace, s_symbol);
        ketl_namespace* p_direct_namespace = ketl_namespace_find_direct_parent(p_namespace, p_symbol_node);
        uint32_t namespace_node_index = ketl_namespace_get_index(p_direct_namespace, p_symbol_node);

        if (p_symbol_node == NULL) {
            p_bucket = hir_builder_symbol_to_var_info_map_t_get_or_insert_copy(
                &p_hir_builder->symbol_to_var_info_stack.p_data[p_hir_builder->symbol_to_var_info_stack.size - 1], 
                fullname, (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size);
            hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
                .p_namespace = p_direct_namespace,
                .namespace_node_index = namespace_node_index,
                .name = name,
            });

            if (force) {
                ketl_hir_var_id_t var_id = (ketl_hir_var_id_t)p_hir_builder->vars.size;
                get_var_info(p_hir_builder, p_bucket->value)->last_var_id = var_id;

                hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
                    .info = p_bucket->value,
                    .type = KETL_HIR_USED_TYPE_UNKNOWN,
                    .uid = KETL_HIR_VAR_UID_GLOBAL,
                    .expr_info = expr_info,
                });
                return var_id;
            } else {
                get_var_info(p_hir_builder, p_bucket->value)->last_var_id = -1;
                return -1;
            }
        }

        p_bucket = hir_builder_symbol_to_var_info_map_t_get_or_insert_copy(
            &p_hir_builder->symbol_to_var_info_stack.p_data[0], 
            fullname, (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size);
        hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
            .p_namespace = p_direct_namespace,
            .namespace_node_index = namespace_node_index,
            .name = name,
        });

        if (p_symbol_node->variable.kind == KETL_VARIABLE_TYPE || p_symbol_node->variable.kind == KETL_VARIABLE_NAMESPACE) {
            known_type = KETL_HIR_USED_TYPE_META;
        } else {
            known_type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_symbol_node->variable.p_type);
        }

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

    ketl_hir_var_id_t var_id = get_var_info(p_hir_builder, p_bucket->value)->last_var_id;
    if (var_id == (ketl_hir_var_id_t)-1) {
        if (force) {
            ketl_hir_var_id_t var_id = (ketl_hir_var_id_t)p_hir_builder->vars.size;
            get_var_info(p_hir_builder, p_bucket->value)->last_var_id = var_id;

            hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
                .info = p_bucket->value,
                .type = KETL_HIR_USED_TYPE_UNKNOWN,
                .uid = KETL_HIR_VAR_UID_GLOBAL,
                .expr_info = expr_info,
            });
            return var_id;
        } else {
            return -1;
        }
    }
    ketl_hir_var_t var = GET_VAR(get_var_info(p_hir_builder, p_bucket->value)->last_var_id);

    var_id = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    get_var_info(p_hir_builder, p_bucket->value)->last_var_id = var_id;

    var.expr_info = expr_info;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, var);

    return var_id;
}

ketl_hir_var_id_t ketl_hir_builder_get_global_var(ketl_hir_builder_t* p_hir_builder, ketl_namespace* p_namespace, ketl_namespace_node* p_namespace_node, ketl_hir_symbol_offset_t name, ketl_hir_expr_info_t expr_info, ketl_hir_used_type_index_t type) {
    hir_builder_symbol_to_var_info_map_t_bucket* p_bucket = find_info_by_symbol(p_hir_builder, name);

    ketl_namespace* p_direct_namespace = ketl_namespace_find_direct_parent(p_namespace, p_namespace_node);
    uint32_t namespace_node_index = ketl_namespace_get_index(p_direct_namespace, p_namespace_node);
    // if size didn't change, we found existing var
    if (p_bucket == NULL) {
        p_bucket = hir_builder_symbol_to_var_info_map_t_get_or_insert_copy(&p_hir_builder->symbol_to_var_info_stack.p_data[0], name, (ketl_hir_var_info_index_t)p_hir_builder->vars_infos.size);
        hir_builder_vars_infos_t_push_back_copy(&p_hir_builder->vars_infos, (ketl_hir_var_info_t){
            .name = name,
            .p_namespace = p_direct_namespace,
            .namespace_node_index = namespace_node_index,
            .last_var_id = (ketl_hir_var_id_t)-1,
        });
    }

    ANN_ASSERT(get_var_info(p_hir_builder, p_bucket->value)->last_var_id == (ketl_hir_var_id_t)-1 ||
        GET_VAR(get_var_info(p_hir_builder, p_bucket->value)->last_var_id).uid == KETL_HIR_VAR_UID_GLOBAL);
    
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
    if (GET_VAR(array_id).type == KETL_HIR_USED_TYPE_UNKNOWN) {
        return ketl_hir_builder_create_temp_var(p_hir_builder, expr_info, KETL_HIR_USED_TYPE_UNKNOWN);
    }

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

ketl_hir_var_id_t ketl_hir_builder_create_field_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t object_id, ketl_hir_symbol_offset_t name, ketl_hir_expr_info_t expr_info, ketl_hir_used_type_index_t field_type) {
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

void ketl_hir_builder_insert_unary_op(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_unary_op_t* p_unary_op, ketl_hir_expr_info_t expr_info) {
    if ((hir_header.tag & KETL_HIR_TYPE_INSTR_MASK) != KETL_HIR_UNDEF && (hir_header.tag & KETL_HIR_TYPE_MASK) == KETL_HIR_UNDEF) {
        // TODO FIX
        // for now we just hash search exact function, later we should take into acount possible implicit casts

        ketl_hir_var_t* p_rhs_var = &GET_VAR(p_unary_op->arg_var);
        
        // if any var is undefined, the op is undefined
        if (p_rhs_var->type == KETL_HIR_USED_TYPE_UNKNOWN) {
            return;
        }

        if (p_rhs_var->type == KETL_HIR_USED_TYPE_LITERAL) {
            p_rhs_var->type = ketl_hir_builder_get_used_type_index(p_hir_builder, 
                ketl_state_get_u64(p_hir_builder->p_state));
        }

        // first type is return type, ignored during search
        ketl_variable_type_info_t a_parameters_array[] = { {.p_type = NULL}, 
            {.p_type = GET_TYPE(p_rhs_var->type)} };
        ketl_function_parameters parameters = {
            .p_parameters = a_parameters_array,
            .parameters_count = ANN_ARRAY_SIZE(a_parameters_array),
        };

        for (size_t i = 0; i < ANN_ARRAY_SIZE(a_parameters_array); ++i) {
            if (a_parameters_array[i].p_type && a_parameters_array[i].p_type->kind == KETL_TYPE_ENUM) {
                a_parameters_array[i].p_type = (ketl_type*)((ketl_type_enum*)a_parameters_array[i].p_type)->p_parent_primitive;
            }
        }

        if (a_parameters_array[1].p_type->kind == KETL_TYPE_CLASS) {
            ketl_type* p_raw_type = ketl_state_get_raw_type(p_hir_builder->p_state);
            a_parameters_array[1].p_type = p_raw_type;
        }

        operator_overloading_map_bucket* p_operator_bucket = operator_overloading_map_get_or_null(
            p_hir_builder->p_state->am_hiroperator_overloading + ((hir_header.tag - KETL_HIR_FIRST_OVERLOADABLE_OPERATOR) >> KETL_HIR_TYPE_INSTR_SHIFT), parameters);
        if (p_operator_bucket == NULL) {
            errorf(expr_info.source_offset, expr_info.length, "Couldn't find proper op overloading.");
            return;
        }
        
        // TODO FIX
        // check if return argument already has defined type and do casting if necessary
        GET_VAR(p_unary_op->output_var).type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_operator_bucket->key.p_parameters[0].p_type);
        hir_header.tag = p_operator_bucket->value;
    }

    ketl_hir_builder_insert_instr(p_hir_builder, hir_header, (uint8_t*)p_unary_op);
}

ketl_hir_tag_t ketl_hir_get_type_tag_from_type(ketl_type* p_type) {
    ANN_ASSERT(p_type);
    if (p_type->kind == KETL_TYPE_ENUM) {
        p_type = (ketl_type*)((ketl_type_enum*)p_type)->p_parent_primitive;
    }
    
    if(p_type->kind == KETL_TYPE_PRIMITIVE) {
        ketl_type_primitive* p_primitive_type = (ketl_type_primitive*)p_type;
        if (p_primitive_type->is_signed) {
            ANN_SWITCH_STRICT (p_primitive_type->size) {
                case 1: return KETL_HIR_I8;
                case 2: return KETL_HIR_I16;
                case 4: return KETL_HIR_I32;
                case 8: return KETL_HIR_I64;
            }
        } else {
            ANN_SWITCH_STRICT (p_primitive_type->size) {
                case 1: return KETL_HIR_U8;
                case 2: return KETL_HIR_U16;
                case 4: return KETL_HIR_U32;
                case 8: return KETL_HIR_U64;
            }
        }
    }

    ANN_ASSERT(sizeof(void*) == 8);
    return KETL_HIR_U64;
}

void ketl_hir_builder_push_assign(ketl_hir_builder_t* p_hir_builder, ketl_hir_tag_t op, ketl_hir_var_id_t lhs_var, ketl_hir_var_id_t rhs_var) {
    ketl_hir_header_t assign_header = {
        .tag = op | ketl_hir_get_type_tag_from_type(GET_TYPE(GET_VAR(lhs_var).type)),
        .file_symbol = p_hir_builder->p_lexer->s_filename,
    };
    ketl_hir_assign_t instr = {
        .dest_var = lhs_var,
        .source_var = rhs_var,
    };
    ketl_hir_builder_insert_instr(p_hir_builder, assign_header, (uint8_t*)&instr);
}

ketl_hir_block_index_t ketl_hir_builder_reserve_blocks(ketl_hir_builder_t* p_hir_builder, uint8_t count) {
    ketl_hir_block_index_t first_block = (ketl_hir_block_index_t)p_hir_builder->blocks.size;
    hir_builder_blocks_t_reserve(&p_hir_builder->blocks, p_hir_builder->blocks.size + count);
    for (uint8_t i = count; i > 0; --i) {
        hir_builder_blocks_t_push_back_copy(&p_hir_builder->blocks, 0u);
    }
    return first_block;
}
void ketl_hir_builder_set_block(ketl_hir_builder_t* p_hir_builder, ketl_hir_block_index_t block) {
    p_hir_builder->blocks.p_data[block] = p_hir_builder->instrs.size;
    hir_builder_offset_to_block_t_get_or_insert_copy(&p_hir_builder->offset_to_block, p_hir_builder->instrs.size, block);
}
ketl_hir_var_id_t ketl_hir_builder_push_hir_cmp_op(ketl_hir_builder_t* p_hir_builder, ketl_hir_tag_t hir_tag, ketl_hir_var_id_t lhs, ketl_hir_var_id_t rhs, ketl_hir_expr_info_t expr_info) {
    ketl_hir_header_t header = {
        .tag = hir_tag,
        .file_symbol = p_hir_builder->p_lexer->s_filename,
    };
    ketl_hir_var_id_t output_var = ketl_hir_builder_create_temp_var(p_hir_builder, expr_info, KETL_HIR_USED_TYPE_UNKNOWN);
    ketl_hir_binary_op_t instr = {
        .output_var = output_var,
        .lhs_var = lhs,
        .rhs_var = rhs,
    };
    ketl_hir_builder_insert_binary_op(p_hir_builder, header, &instr, expr_info);
    return output_var;
}

void ketl_hir_builder_push_if(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t bool_expr_var, ketl_hir_block_index_t true_statement, ketl_hir_block_index_t false_statement) {
    // TODO token_check casting
    
    ketl_hir_header_t if_header = {
        .tag = KETL_HIR_JUMP_IF_TRUE,
        .file_symbol = p_hir_builder->p_lexer->s_filename,
    };
    ketl_hir_jump_if_t instr = {
        .true_block = true_statement,
        .false_block = false_statement,
        .expr_var = bool_expr_var,
    };
    
    // TODO provide true and false branches
    ketl_hir_builder_insert_instr(p_hir_builder, if_header, (uint8_t*)&instr);
}

void ketl_hir_builder_push_jump(ketl_hir_builder_t* p_hir_builder, ketl_hir_block_index_t target) {
    ketl_hir_header_t jump_header = {
        .tag = KETL_HIR_JUMP,
        .file_symbol = p_hir_builder->p_lexer->s_filename,
    };
    ketl_hir_jump_t instr = {
        .block_index = target,
    };
    
    ketl_hir_builder_insert_instr(p_hir_builder, jump_header, (uint8_t*)&instr);
}

ketl_hir_var_id_t ketl_hir_builder_push_bool_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_expr_info_t expr_info, bool value) {
    ketl_type* p_type = ketl_state_get_bool_type(p_hir_builder->p_state);
    ketl_hir_used_type_index_t type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_type);
    ketl_hir_symbol_offset_t symbol = (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_hir_builder->symbols, value ? "1" : "0", 1);
    return ketl_hir_builder_get_literal(p_hir_builder, symbol, expr_info, type);
}

void ketl_hir_builder_insert_binary_op(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_binary_op_t* p_binary_op, ketl_hir_expr_info_t expr_info) {
    if ((hir_header.tag & KETL_HIR_TYPE_INSTR_MASK) != KETL_HIR_UNDEF && (hir_header.tag & KETL_HIR_TYPE_MASK) == KETL_HIR_UNDEF) {
        // TODO FIX
        // for now we just hash search exact function, later we should take into acount possible implicit casts

        ketl_hir_var_t* p_lhs_var = &GET_VAR(p_binary_op->lhs_var);
        ketl_hir_var_t* p_rhs_var = &GET_VAR(p_binary_op->rhs_var);

        if (p_lhs_var->type == KETL_HIR_USED_TYPE_LITERAL &&
            p_rhs_var->type == KETL_HIR_USED_TYPE_LITERAL) {
            // TODO calculate and put result as literal
            ketl_hir_used_type_index_t type = ketl_hir_builder_get_used_type_index(p_hir_builder, ketl_state_get_i64(p_hir_builder->p_state));
            p_lhs_var->type = type;
            p_rhs_var->type = type;
        } else if (p_lhs_var->type == KETL_HIR_USED_TYPE_LITERAL) {
            p_lhs_var->type = p_rhs_var->type;
        } else if (p_rhs_var->type == KETL_HIR_USED_TYPE_LITERAL) {
            p_rhs_var->type = p_lhs_var->type;
        }

        // if any var is undefined, the op is undefined
        if (p_lhs_var->type == KETL_HIR_USED_TYPE_UNKNOWN ||
            p_rhs_var->type == KETL_HIR_USED_TYPE_UNKNOWN) {
            return;
        }

        // first type is return type, ignored during search
        ketl_variable_type_info_t a_parameters_array[] = { {.p_type = NULL}, 
            {.p_type = GET_TYPE(p_lhs_var->type)}, 
            {.p_type = GET_TYPE(p_rhs_var->type)} };
        ketl_function_parameters parameters = {
            .p_parameters = a_parameters_array,
            .parameters_count = ANN_ARRAY_SIZE(a_parameters_array),
        };

        for (size_t i = 0; i < ANN_ARRAY_SIZE(a_parameters_array); ++i) {
            if (a_parameters_array[i].p_type && a_parameters_array[i].p_type->kind == KETL_TYPE_ENUM) {
                a_parameters_array[i].p_type = (ketl_type*)((ketl_type_enum*)a_parameters_array[i].p_type)->p_parent_primitive;
            }
        }

        if (a_parameters_array[1].p_type->kind == KETL_TYPE_PRIMITIVE && a_parameters_array[2].p_type->kind == KETL_TYPE_PRIMITIVE) {
            ketl_type_primitive* p_lhs_type = (ketl_type_primitive*)a_parameters_array[1].p_type;
            ketl_type_primitive* p_rhs_type = (ketl_type_primitive*)a_parameters_array[2].p_type;
            if (p_lhs_type->is_numeric && p_rhs_type->is_numeric && p_lhs_type->is_signed == p_rhs_type->is_signed) {
                if (p_lhs_type->size < p_rhs_type->size) {
                    p_binary_op->lhs_var = ketl_hir_builder_cast_primitive(p_hir_builder, p_binary_op->lhs_var, p_rhs_var->type);
                    a_parameters_array[1].p_type = a_parameters_array[2].p_type;
                } else if (p_rhs_type->size < p_lhs_type->size) {
                    p_binary_op->rhs_var = ketl_hir_builder_cast_primitive(p_hir_builder, p_binary_op->rhs_var, p_lhs_var->type);
                    a_parameters_array[2].p_type = a_parameters_array[1].p_type;
                }
            }
        }

        if (ketl_type_is_pointer_type(a_parameters_array[1].p_type) && ketl_type_is_raw_type(a_parameters_array[2].p_type)) {
            a_parameters_array[1].p_type = a_parameters_array[2].p_type;
        } else if (ketl_type_is_pointer_type(a_parameters_array[2].p_type) && ketl_type_is_raw_type(a_parameters_array[1].p_type)) {
            a_parameters_array[1].p_type = a_parameters_array[2].p_type;
        }

        if (a_parameters_array[1].p_type->kind == KETL_TYPE_CLASS && 
            a_parameters_array[1].p_type == a_parameters_array[2].p_type) {
            ketl_type* p_raw_type = ketl_state_get_raw_type(p_hir_builder->p_state);
            a_parameters_array[1].p_type = p_raw_type;
            a_parameters_array[2].p_type = p_raw_type;
        }

        if (a_parameters_array[1].p_type->kind == KETL_TYPE_ARRAY && a_parameters_array[2].p_type->kind == KETL_TYPE_ARRAY &&
            ((ketl_type_array*)a_parameters_array[1].p_type)->p_value_type == ((ketl_type_array*)a_parameters_array[2].p_type)->p_value_type) {
            
            ketl_hir_var_id_t output_var = p_binary_op->output_var;
            GET_VAR(output_var).type = ketl_hir_builder_get_used_type_index(p_hir_builder, ketl_state_get_bool_type(p_hir_builder->p_state));
            
            ketl_hir_var_id_t lhs_var = p_binary_op->lhs_var;
            ketl_hir_var_id_t rhs_var = p_binary_op->rhs_var;

            ketl_hir_block_index_t first_block = ketl_hir_builder_reserve_blocks(p_hir_builder, 7);

            ketl_hir_used_type_index_t size_type = ketl_hir_builder_get_used_type_index(p_hir_builder, ketl_state_get_u64(p_hir_builder->p_state));
            ketl_hir_symbol_offset_t size_symbol = (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_hir_builder->symbols, "size", 4);
            
            ketl_hir_var_id_t lhs_size_var = ketl_hir_builder_create_field_var(p_hir_builder, lhs_var, size_symbol, expr_info, size_type);
            ketl_hir_var_id_t rhs_size_var = ketl_hir_builder_create_field_var(p_hir_builder, rhs_var, size_symbol, expr_info, size_type);

            ketl_hir_var_id_t is_sizes_not_equal_var = ketl_hir_builder_push_hir_cmp_op(p_hir_builder, KETL_HIR_NOT_EQUAL, lhs_size_var, rhs_size_var, expr_info);
            ketl_hir_builder_push_if(p_hir_builder, is_sizes_not_equal_var, first_block + 4, first_block + 0);

            ketl_hir_builder_set_block(p_hir_builder, first_block + 0);
            ketl_hir_var_id_t index_var = ketl_hir_builder_create_temp_var(p_hir_builder, expr_info, size_type);
            ketl_hir_var_id_t zero_var = ketl_hir_builder_get_literal(p_hir_builder, KETL_HIR_LITERAL_NULL, expr_info, size_type);
            ketl_hir_builder_push_assign(p_hir_builder, KETL_HIR_ASSIGN, index_var, zero_var);
            ketl_hir_builder_push_jump(p_hir_builder, first_block + 1);

            ketl_hir_builder_set_block(p_hir_builder, first_block + 1);
            ketl_hir_var_id_t is_index_less_size_var = ketl_hir_builder_push_hir_cmp_op(p_hir_builder, KETL_HIR_LESS, index_var, lhs_size_var, expr_info);
            ketl_hir_builder_push_if(p_hir_builder, is_index_less_size_var, first_block + 2, first_block + 5);

            ketl_hir_builder_set_block(p_hir_builder, first_block + 2);
            ketl_hir_var_id_t lhs_indexed_var = ketl_hir_builder_create_index_var(p_hir_builder, lhs_var, index_var, expr_info);
            ketl_hir_var_id_t rhs_indexed_var = ketl_hir_builder_create_index_var(p_hir_builder, rhs_var, index_var, expr_info);
            ketl_hir_var_id_t is_values_not_equal_var = ketl_hir_builder_push_hir_cmp_op(p_hir_builder, KETL_HIR_NOT_EQUAL, lhs_indexed_var, rhs_indexed_var, expr_info);
            ketl_hir_builder_push_if(p_hir_builder, is_values_not_equal_var, first_block + 4, first_block + 3);

            ketl_hir_builder_set_block(p_hir_builder, first_block + 3);
            ketl_hir_var_id_t one_var = ketl_hir_builder_get_literal(p_hir_builder, 
                (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_hir_builder->symbols, "1", 1), expr_info, size_type);
            ketl_hir_builder_push_assign(p_hir_builder, KETL_HIR_ASSIGN_PLUS, index_var, one_var);
            ketl_hir_builder_push_jump(p_hir_builder, first_block + 1);

            ketl_hir_builder_set_block(p_hir_builder, first_block + 4);
            ketl_hir_var_id_t false_var = ketl_hir_builder_push_bool_var(p_hir_builder, expr_info, false);
            ketl_hir_builder_push_assign(p_hir_builder, KETL_HIR_ASSIGN, output_var, false_var);
            ketl_hir_builder_push_jump(p_hir_builder, first_block + 6);

            ketl_hir_builder_set_block(p_hir_builder, first_block + 5);
            ketl_hir_var_id_t true_var = ketl_hir_builder_push_bool_var(p_hir_builder, expr_info, true);
            ketl_hir_builder_push_assign(p_hir_builder, KETL_HIR_ASSIGN, output_var, true_var);
            ketl_hir_builder_push_jump(p_hir_builder, first_block + 6);

            ketl_hir_builder_set_block(p_hir_builder, first_block + 6);
            return;
        }

        operator_overloading_map_bucket* p_operator_bucket = operator_overloading_map_get_or_null(
            p_hir_builder->p_state->am_hiroperator_overloading + ((hir_header.tag - KETL_HIR_FIRST_OVERLOADABLE_OPERATOR) >> KETL_HIR_TYPE_INSTR_SHIFT), parameters);
        if (p_operator_bucket == NULL) {
            errorf(expr_info.source_offset, expr_info.length, "Couldn't find proper op overloading.");
            return;
        }
        
        // TODO FIX
        // check if return argument already has defined type and do casting if necessary
        GET_VAR(p_binary_op->output_var).type = ketl_hir_builder_get_used_type_index(p_hir_builder, p_operator_bucket->key.p_parameters[0].p_type);
        hir_header.tag = p_operator_bucket->value;
    }

    ketl_hir_builder_insert_instr(p_hir_builder, hir_header, (uint8_t*)p_binary_op);
}

void ketl_hir_builder_insert_call(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_call_t* p_call, ketl_hir_var_id_t* p_arguments) {
    ketl_hir_var_t* p_callee = &GET_VAR(p_call->callee);

    if (p_callee->type == KETL_HIR_USED_TYPE_UNKNOWN) {
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
    ketl_namespace_node* p_type_node = ketl_namespace_find_by_index(
        get_var_info(p_hir_builder, GET_VAR(p_create->type_var).info)->p_namespace,
        get_var_info(p_hir_builder, GET_VAR(p_create->type_var).info)->namespace_node_index);
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

ketl_hir_var_id_t ketl_hir_builder_cast_primitive(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t var, ketl_hir_used_type_index_t target_type) {
    ketl_type* p_lhs_type = GET_TYPE(target_type); 
    ketl_type* p_rhs_type = GET_TYPE(GET_VAR(var).type); 

    ANN_ASSERT((p_lhs_type->kind == KETL_TYPE_PRIMITIVE && p_rhs_type->kind == KETL_TYPE_PRIMITIVE) ||
        (ketl_type_is_pointer_type(p_lhs_type) && ketl_type_is_raw_type(p_rhs_type)) || 
        (ketl_type_is_pointer_type(p_rhs_type) && ketl_type_is_raw_type(p_lhs_type)));

    ketl_hir_var_id_t casted_var = (ketl_hir_var_id_t)p_hir_builder->vars.size;
    hir_builder_vars_t_push_back_copy(&p_hir_builder->vars, (ketl_hir_var_t){
        .cast_target = var,
        .type = target_type,
        .uid = KETL_HIR_VAR_UID_CAST,
        .expr_info = GET_VAR(var).expr_info,
    });
    
    return casted_var;
}

ketl_hir_const_index_t ketl_hir_builder_push_string_literal(ketl_hir_builder_t* p_hir_builder, ketl_token_t literal) {
    ketl_hir_const_index_t literal_index = p_hir_builder->consts_infos.size;
    char a_name_buffer[256];
    uint32_t name_length = (uint32_t)snprintf(a_name_buffer, ANN_ARRAY_SIZE(a_name_buffer), ".L__const.str%"PRIu16"_%"PRIu16, p_hir_builder->func_index, p_hir_builder->string_literal_counter++);

    uint32_t const_offset = p_hir_builder->consts.size;
    uint32_t const_size = TOKEN_LENGTH(literal);

    // TODO does not comply with escaped chars
    ketl_hir_consts_t_push_back_ref_n(&p_hir_builder->consts, (const uint8_t*)TOKEN_STRING(literal), TOKEN_LENGTH(literal));

    ketl_hir_consts_infos_t_push_back_copy(&p_hir_builder->consts_infos, (ketl_hir_const_info_t){
        .const_offset = const_offset,
        .const_size = const_size,
        .s_name = (ketl_hir_symbol_offset_t)ketl_atomic_strings_get(&p_hir_builder->p_state->atomic_strings, a_name_buffer, name_length),
        .is_string = true,
    });

    return literal_index;
}
