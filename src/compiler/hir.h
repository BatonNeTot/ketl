//🫖ketl
#ifndef ketl_compiler_hir_h
#define ketl_compiler_hir_h

#include "type_impl.h"

#include "variable.h"

#include "containers/vector.h"
#include "containers/hash_map.h"
#include "atomic_strings.h"

#include "ketl/utils.h"

typedef uint8_t ketl_hir_tag_t;

enum {
    KETL_HIR_NONE_STMT,

    KETL_HIR_PLUS_UNDEF,
    KETL_HIR_MINUS_UNDEF,
    KETL_HIR_MULTY_UNDEF,
    KETL_HIR_DIV_UNDEF,

    KETL_HIR_PLUS_I64,
    KETL_HIR_MINUS_I64,
    KETL_HIR_MULTY_I64,
    KETL_HIR_DIV_I64,

    KETL_HIR_CALL_VOID,
    KETL_HIR_CALL,

    KETL_HIR_ASSIGN,
    
    KETL_HIR_RETURN,
    KETL_HIR_RETURN_VALUE,

    // only count binary operators for now
    KETL_HIR_FIRST_UNDEF_OPERATOR = KETL_HIR_PLUS_UNDEF,
    KETL_HIR_LAST_UNDEF_OPERATOR = KETL_HIR_DIV_UNDEF,
};

bool ketl_hir_is_terminator_tag(ketl_hir_tag_t tag);

////////////////////////////////

typedef uint32_t ketl_hir_instr_offset_t;
typedef uint16_t ketl_hir_var_id_t;
typedef uint16_t ketl_hir_block_index_t;
typedef uint16_t ketl_hir_used_type_index_t;
typedef uint16_t ketl_hir_symbol_offset_t;
typedef uint16_t ketl_hir_used_global_index_t;

KETL_DEFINE(ketl_hir_header_t) {
    ketl_hir_tag_t tag;
    ketl_hir_symbol_offset_t file_symbol;
    uint32_t start_line_index;
    uint32_t end_line_index;
    uint16_t start_col_index;
    uint16_t end_col_index;
};

KETL_DEFINE(ketl_hir_binary_op_t) {
    ketl_hir_var_id_t lhs_var;
    ketl_hir_var_id_t rhs_var;
    ketl_hir_var_id_t output_var;
};

KETL_DEFINE(ketl_hir_call_void_t) {
    ketl_hir_var_id_t callee;
    uint16_t arguments_count;
    ketl_hir_var_id_t arguments[];
};

KETL_DEFINE(ketl_hir_call_t) {
    ketl_hir_var_id_t output_var;
    ketl_hir_var_id_t callee;
    uint16_t arguments_count;
    ketl_hir_var_id_t arguments[];
};

KETL_DEFINE(ketl_hir_assign_t) {
    ketl_hir_var_id_t dest_var;
    ketl_hir_var_id_t source_var;
};

KETL_DEFINE(ketl_hir_return_value_t) {
    ketl_hir_var_id_t value_var;
};

////////////////////////////////

#define KETL_HIR_VAR_UID_LITERAL ((uint16_t)-1)
#define KETL_HIR_VAR_UID_GLOBAL ((uint16_t)-2)
#define KETL_HIR_USED_TYPE_UNKHOWN ((uint16_t)-1)
#define KETL_HIR_VAR_NAME_TEMP KETL_ATOMIC_STRING_EMPTY

KETL_DEFINE(ketl_hir_var_t) {
    union {
        ketl_hir_symbol_offset_t name;
        ketl_hir_used_global_index_t global_index;
    };
    ketl_hir_used_type_index_t type;
    uint16_t uid;
};

KETL_DEFINE(ketl_hir_global_t) {
    ketl_variable* p_variable;
    ketl_hir_symbol_offset_t name;
};

KETL_DEFINE(ketl_hir_t) {
    const ketl_allocator* p_allocator;

    uint8_t* p_instrs;
    ketl_hir_var_t* p_vars;
    ketl_hir_global_t* p_used_globals;
    ketl_hir_instr_offset_t* p_block_offsets;
    ketl_type** p_used_types;
    char* p_symbols;
    
    ketl_hir_instr_offset_t instrs_count;
    ketl_hir_var_id_t vars_count;
    ketl_hir_used_global_index_t used_global_count;
    ketl_hir_block_index_t blocks_count;
    ketl_hir_used_type_index_t used_types_count;
};

void ketl_hir_deinit(ketl_hir_t* p_hir);

ketl_hir_instr_offset_t ketl_hir_get_instr_size(ketl_hir_tag_t tag, uint8_t* p_instr);

ketl_hir_instr_offset_t ketl_hir_decode_size(ketl_hir_t* p_hir, ketl_hir_instr_offset_t instr_offset);

uint32_t ketl_hir_format(ketl_hir_t* p_hir, char* p_buffer, uint32_t buffer_size);

//////////////////////////////////

KETL_FORWARD(ketl_state);

KETL_VECTOR_DECLARATION(hir_builder_instrs_t, uint8_t)
KETL_VECTOR_DECLARATION(hir_builder_vars_t, ketl_hir_var_t)
KETL_VECTOR_DECLARATION(hir_builder_used_globals_t, ketl_hir_global_t)
KETL_VECTOR_DECLARATION(hir_builder_blocks_t, ketl_hir_instr_offset_t)
KETL_VECTOR_DECLARATION(hir_builder_used_types_t, ketl_type*)

KETL_HASH_MAP_DECLARATION(hir_builder_symbol_to_var_map_t, ketl_hir_symbol_offset_t, ketl_hir_var_id_t)
KETL_HASH_MAP_DECLARATION(hir_builder_type_to_used_type_map_t, ketl_type*, ketl_hir_used_type_index_t)

KETL_DEFINE(ketl_hir_builder_t) {
    const ketl_allocator* p_allocator;

    hir_builder_instrs_t v_instrs;
    hir_builder_vars_t v_vars;
    hir_builder_used_globals_t v_used_globals;
    hir_builder_blocks_t v_blocks;
    hir_builder_used_types_t v_used_types;
    ketl_atomic_strings symbols;

    hir_builder_symbol_to_var_map_t m_symbol_to_var;
    hir_builder_type_to_used_type_map_t m_type_to_used_type;

    ketl_hir_block_index_t active_block;
};

void ketl_hir_builder_init(ketl_hir_builder_t* p_hir_builder, const ketl_allocator* p_allocator);

void ketl_hir_builder_flush(ketl_hir_builder_t* p_hir_builder, ketl_hir_t* p_hir);

ketl_hir_used_type_index_t ketl_hir_builder_get_used_type_index(ketl_hir_builder_t* p_hir_builder, ketl_type* p_type);

ketl_hir_var_id_t ketl_hir_builder_get_var(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t name, ketl_hir_used_type_index_t type);

ketl_hir_var_id_t ketl_hir_builder_get_literal(ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t literal, ketl_hir_used_type_index_t type);

ketl_hir_var_id_t ketl_hir_builder_register_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_symbol_offset_t name, ketl_hir_used_type_index_t type);

ketl_hir_var_id_t ketl_hir_builder_create_temp_var(ketl_hir_builder_t* p_hir_builder, ketl_hir_used_type_index_t type);

void ketl_hir_builder_insert_instr(ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, uint8_t* p_instr);

void ketl_hir_builder_insert_binary_op(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_binary_op_t* p_binary_op);

void ketl_hir_builder_insert_call(ketl_state* p_state, ketl_hir_builder_t* p_hir_builder, ketl_hir_header_t hir_header, ketl_hir_call_t* p_call, ketl_hir_var_id_t* p_arguments);

#endif
