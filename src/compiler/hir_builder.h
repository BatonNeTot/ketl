//🫖ketl
#ifndef ketl_compiler_hir_builder_h
#define ketl_compiler_hir_builder_h

#include "hir.h"

#include "containers/vector.h"
#include "containers/hash_map.h"
#include "atomic_strings.h"

ANN_FORWARD(ketl_state);
ANN_FORWARD(ketl_namespace);

KETL_VECTOR_DECLARATION(hir_builder_instrs_t, uint8_t)
KETL_VECTOR_DECLARATION(hir_builder_vars_t, ketl_hir_var_t)
KETL_VECTOR_DECLARATION(hir_builder_vars_infos_t, ketl_hir_var_info_t)
KETL_VECTOR_DECLARATION(hir_builder_blocks_t, ketl_hir_instr_offset_t)
KETL_VECTOR_DECLARATION(hir_builder_used_types_t, ketl_type*)

KETL_HASH_MAP_DECLARATION(hir_builder_symbol_to_var_map_t, ketl_hir_symbol_offset_t, ketl_hir_var_id_t)
KETL_HASH_MAP_DECLARATION(hir_builder_type_to_used_type_map_t, ketl_type*, ketl_hir_used_type_index_t)
KETL_HASH_MAP_DECLARATION(hir_builder_offset_to_block_t, ketl_hir_instr_offset_t, ketl_hir_block_index_t)

KETL_VECTOR_DECLARATION(hir_builder_return_offsets_t, ketl_hir_instr_offset_t)

ANN_DEFINE(hir_builder_block_info_t) {
    bool reachable;
};

KETL_VECTOR_DECLARATION(hir_builder_blocks_infos_t, hir_builder_block_info_t)

ANN_DEFINE(ketl_hir_builder_t) {
    const ketl_allocator* p_allocator;

    hir_builder_instrs_t instrs;
    hir_builder_vars_t vars;
    hir_builder_vars_infos_t vars_infos;
    hir_builder_blocks_t blocks;
    hir_builder_used_types_t used_types;
    ketl_atomic_strings symbols;

    hir_builder_return_offsets_t return_offsets;
    hir_builder_blocks_infos_t blocks_infos;

    hir_builder_symbol_to_var_map_t symbol_to_var;
    hir_builder_type_to_used_type_map_t type_to_used_type;
    hir_builder_offset_to_block_t offset_to_block;
   
    ketl_hir_var_id_t parameter_count; 
    bool has_calls;
};

void ketl_hir_builder_init(ketl_hir_builder_t* p_hir_builder, const ketl_allocator* p_allocator);

void ketl_hir_builder_add_parameter(ketl_state* p_state, ketl_namespace* p_namespace, ketl_hir_builder_t* p_hir_builder, ketl_named_variable_type_info_t* p_parameter_info);

void ketl_hir_builder_flush(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_t* p_hir);

ketl_hir_used_type_index_t ketl_hir_builder_get_used_type_index(ketl_hir_builder_t* p_hir_builder, ketl_type* p_type);

ketl_hir_var_id_t ketl_hir_builder_get_literal(ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t literal, ketl_hir_used_type_index_t type);

ketl_hir_var_id_t ketl_hir_builder_register_var(ketl_state* p_state, ketl_namespace* p_namespace, ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t name, ketl_hir_used_type_index_t type);

ketl_hir_var_id_t ketl_hir_builder_get_var(ketl_state* p_state, ketl_namespace* p_namespace, ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t name, ketl_hir_used_type_index_t type);

ketl_hir_var_id_t ketl_hir_builder_increment_var_uid(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t var_id);

ketl_hir_var_id_t ketl_hir_builder_create_index_var(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t array_id, ketl_hir_var_id_t arg_id);

ketl_hir_var_id_t ketl_hir_builder_create_field_var(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t object_id, ketl_hir_symbol_offset_t name, ketl_hir_used_type_index_t type);

ketl_hir_var_id_t ketl_hir_builder_create_temp_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_used_type_index_t type);

void ketl_hir_builder_replace_temp_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_var_id_t donor_var, ketl_hir_var_id_t temp_var);

void ketl_hir_builder_insert_instr(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, uint8_t* p_instr);

void ketl_hir_builder_insert_binary_op(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_binary_op_t* p_binary_op);

void ketl_hir_builder_insert_call(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_call_t* p_call, ketl_hir_var_id_t* p_arguments);

void ketl_hir_builder_insert_create(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_create_t* p_create, ketl_hir_var_id_t* p_arguments);

#endif
