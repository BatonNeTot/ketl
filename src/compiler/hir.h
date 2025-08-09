//🫖ketl
#ifndef ketl_compiler_hir_h
#define ketl_compiler_hir_h

#include "type_impl.h"

#include "variable.h"

#include "ketl/utils.h"

typedef uint8_t ketl_hir_tag_t;

enum {
    KETL_HIR_NONE_STMT,

    KETL_HIR_PLUS_UNDEF,
    KETL_HIR_MINUS_UNDEF,
    KETL_HIR_MULTY_UNDEF,
    KETL_HIR_DIV_UNDEF,
    KETL_HIR_MOD_UNDEF,

    KETL_HIR_EQUAL_UNDEF,
    KETL_HIR_NOT_EQUAL_UNDEF,
    KETL_HIR_LESS_UNDEF,
    KETL_HIR_LESS_OR_EQUAL_UNDEF,
    KETL_HIR_GREATER_UNDEF,
    KETL_HIR_GREATER_OR_EQUAL_UNDEF,

    KETL_HIR_PLUS_I64,
    KETL_HIR_MINUS_I64,
    KETL_HIR_MULTY_I64,
    KETL_HIR_DIV_I64,
    KETL_HIR_MOD_I64,

    KETL_HIR_EQUAL_I64,
    KETL_HIR_NOT_EQUAL_I64,
    KETL_HIR_LESS_I64,
    KETL_HIR_LESS_OR_EQUAL_I64,
    KETL_HIR_GREATER_I64,
    KETL_HIR_GREATER_OR_EQUAL_I64,

    KETL_HIR_CALL_VOID,
    KETL_HIR_CALL,

    KETL_HIR_ASSIGN,

    KETL_HIR_JUMP,

    KETL_HIR_JUMP_IF,
    
    KETL_HIR_RETURN,
    KETL_HIR_RETURN_VALUE,

    // only count binary operators for now
    KETL_HIR_FIRST_UNDEF_OPERATOR = KETL_HIR_PLUS_UNDEF,
    KETL_HIR_LAST_UNDEF_OPERATOR = KETL_HIR_GREATER_OR_EQUAL_UNDEF,
};

bool ketl_hir_is_terminator_tag(ketl_hir_tag_t tag);

////////////////////////////////

typedef uint32_t ketl_hir_instr_offset_t;
typedef uint16_t ketl_hir_var_id_t;
typedef uint16_t ketl_hir_var_info_index_t;
typedef uint16_t ketl_hir_block_index_t;
typedef uint16_t ketl_hir_used_type_index_t;
typedef uint16_t ketl_hir_symbol_offset_t;

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

KETL_DEFINE(ketl_hir_jump_t) {
    ketl_hir_block_index_t block_index;
};

KETL_DEFINE(ketl_hir_jump_if_t) {
    ketl_hir_block_index_t true_block;
    ketl_hir_block_index_t false_block;
    ketl_hir_var_id_t expr_var;
};

KETL_DEFINE(ketl_hir_return_value_t) {
    ketl_hir_var_id_t value_var;
};

////////////////////////////////

typedef uint16_t ketl_hir_var_uid_t;

#define KETL_HIR_VAR_UID_LITERAL ((ketl_hir_var_uid_t)-1)
#define KETL_HIR_USED_TYPE_UNKHOWN ((ketl_hir_used_type_index_t)-1)
#define KETL_HIR_VAR_INFO_TEMP ((ketl_hir_var_info_index_t)-1)
#define KETL_HIR_VAR_NAME_TEMP KETL_ATOMIC_STRING_EMPTY

KETL_DEFINE(ketl_hir_var_t) {
    union {
        ketl_hir_symbol_offset_t literal;
        ketl_hir_var_info_index_t info;
    };
    ketl_hir_used_type_index_t type;
    ketl_hir_var_uid_t uid;
};

KETL_DEFINE(ketl_hir_var_info_t) {
    ketl_hir_symbol_offset_t name;
    ketl_variable* p_global;
    // TODO declaration info
};

KETL_DEFINE(ketl_hir_t) {
    const ketl_allocator* p_allocator;

    uint8_t* p_instrs;
    ketl_hir_var_t* p_vars;
    ketl_hir_var_info_t* p_vars_infos;
    ketl_hir_instr_offset_t* p_block_offsets;
    ketl_type** p_used_types;
    char* p_symbols;
    
    ketl_hir_instr_offset_t instrs_count;
    ketl_hir_var_id_t vars_count;
    ketl_hir_var_info_index_t vars_infos_count;
    ketl_hir_block_index_t blocks_count;
    ketl_hir_used_type_index_t used_types_count;
};

void ketl_hir_deinit(ketl_hir_t* p_hir);

ketl_hir_instr_offset_t ketl_hir_get_instr_size(ketl_hir_tag_t tag, uint8_t* p_instr);

ketl_hir_instr_offset_t ketl_hir_decode_size(ketl_hir_t* p_hir, ketl_hir_instr_offset_t instr_offset);

uint32_t ketl_hir_format(ketl_hir_t* p_hir, char* p_buffer, uint32_t buffer_size);

#endif
