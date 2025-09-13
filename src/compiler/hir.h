//🫖ketl
#ifndef ketl_compiler_hir_h
#define ketl_compiler_hir_h

#include "type_impl.h"

#include "variable.h"

#include "ketl/utils.h"


typedef uint16_t ketl_hir_tag_t;

enum {
    // types

#define KETL_HIR_TYPE_MASK              0x000F

    KETL_HIR_UNDEF                    = 0x0000,

    KETL_HIR_U8                       = 0x0004,
    KETL_HIR_U16,
    KETL_HIR_U32,
    KETL_HIR_U64,
    
    KETL_HIR_I8,
    KETL_HIR_I16,
    KETL_HIR_I32,
    KETL_HIR_I64,

    KETL_HIR_F32                      = 0x000E,
    KETL_HIR_F64,

    // type independent instructions

    KETL_HIR_NONE_STMT                = 0x0000,

    KETL_HIR_CALL_VOID,
    KETL_HIR_CALL,

    KETL_HIR_JUMP,

    KETL_HIR_JUMP_IF_TRUE,
    KETL_HIR_JUMP_IF_FALSE,

    KETL_HIR_RETURN,

    // type dependent instructions

#define KETL_HIR_TYPE_INSTR_MASK        0xFFF0
#define KETL_HIR_TYPE_INSTR_SHIFT       4

    KETL_HIR_PLUS                     = 0x0010,
    KETL_HIR_MINUS                    = 0x0020,
    KETL_HIR_MULTY                    = 0x0030,
    KETL_HIR_DIV                      = 0x0040,
    KETL_HIR_MOD                      = 0x0050,

    KETL_HIR_EQUAL                    = 0x0060,
    KETL_HIR_NOT_EQUAL                = 0x0070,
    KETL_HIR_LESS                     = 0x0080,
    KETL_HIR_LESS_OR_EQUAL            = 0x0090,
    KETL_HIR_GREATER                  = 0x00A0,
    KETL_HIR_GREATER_OR_EQUAL         = 0x00B0,

    KETL_HIR_ASSIGN                   = 0x00C0,
    
    KETL_HIR_JUMP_IF_EQUAL            = 0x00D0,
    KETL_HIR_JUMP_IF_NOT_EQAUL        = 0x00E0,
    
    KETL_HIR_JUMP_IF_LESS             = 0x00F0,
    KETL_HIR_JUMP_IF_LESS_OR_EQAUL    = 0x0100,
    
    KETL_HIR_JUMP_IF_GREATER          = 0x0110,
    KETL_HIR_JUMP_IF_GREATER_OR_EQAUL = 0x0120,

    KETL_HIR_RETURN_VALUE             = 0x0130,

    KETL_HIR_FIRST_BI_OPERATOR = KETL_HIR_PLUS,
    KETL_HIR_LAST_BI_OPERATOR = KETL_HIR_GREATER_OR_EQUAL,
};

bool ketl_hir_is_terminator_tag(ketl_hir_tag_t tag);

////////////////////////////////

typedef uint32_t ketl_hir_instr_offset_t;
typedef uint16_t ketl_hir_var_id_t;
typedef uint16_t ketl_hir_var_info_index_t;
typedef uint16_t ketl_hir_block_index_t;
typedef uint16_t ketl_hir_used_type_index_t;
typedef uint16_t ketl_hir_symbol_offset_t;

ANN_DEFINE(ketl_hir_header_t) {
    ketl_hir_tag_t tag;
    ketl_hir_symbol_offset_t file_symbol;
};

ANN_DEFINE(ketl_hir_binary_op_t) {
    ketl_hir_var_id_t lhs_var;
    ketl_hir_var_id_t rhs_var;
    ketl_hir_var_id_t output_var;
};

ANN_DEFINE(ketl_hir_call_void_t) {
    ketl_hir_var_id_t callee;
    uint16_t arguments_count;
    ketl_hir_var_id_t arguments[];
};

ANN_DEFINE(ketl_hir_call_t) {
    ketl_hir_var_id_t output_var;
    ketl_hir_var_id_t callee;
    uint16_t arguments_count;
    ketl_hir_var_id_t arguments[];
};

ANN_DEFINE(ketl_hir_assign_t) {
    ketl_hir_var_id_t dest_var;
    ketl_hir_var_id_t source_var;
};

ANN_DEFINE(ketl_hir_jump_t) {
    ketl_hir_block_index_t block_index;
};

ANN_DEFINE(ketl_hir_jump_if_t) {
    ketl_hir_block_index_t true_block;
    ketl_hir_block_index_t false_block;
    ketl_hir_var_id_t expr_var;
};

ANN_DEFINE(ketl_hir_jump_if_cmp_t) {
    ketl_hir_block_index_t true_block;
    ketl_hir_block_index_t false_block;
    ketl_hir_var_id_t lhs_var;
    ketl_hir_var_id_t rhs_var;
};

ANN_DEFINE(ketl_hir_return_value_t) {
    ketl_hir_var_id_t value_var;
};

////////////////////////////////

typedef uint16_t ketl_hir_var_uid_t;

#define KETL_HIR_VAR_UID_LITERAL ((ketl_hir_var_uid_t)-1)
#define KETL_HIR_USED_TYPE_UNKNOWN ((ketl_hir_used_type_index_t)-1)
#define KETL_HIR_VAR_INFO_TEMP ((ketl_hir_var_info_index_t)-1)
#define KETL_HIR_VAR_NAME_TEMP KETL_ATOMIC_STRING_EMPTY

ANN_DEFINE(ketl_hir_var_t) {
    union {
        ketl_hir_symbol_offset_t literal;
        ketl_hir_var_info_index_t info;
    };
    ketl_hir_used_type_index_t type;
    ketl_hir_var_uid_t uid;
};

ANN_DEFINE(ketl_hir_var_info_t) {
    ketl_hir_symbol_offset_t name;
    ketl_variable* p_global;
    // TODO declaration info
};

ANN_DEFINE(ketl_hir_t) {
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

    ketl_hir_used_type_index_t return_type;
    bool has_calls;
};

void ketl_hir_deinit(ketl_hir_t* p_hir);

ketl_hir_instr_offset_t ketl_hir_get_instr_size(ketl_hir_tag_t tag, uint8_t* p_instr);

ketl_hir_instr_offset_t ketl_hir_decode_size(ketl_hir_t* p_hir, ketl_hir_instr_offset_t instr_offset);

uint32_t ketl_hir_format(ketl_hir_t* p_hir, char* p_buffer, uint32_t buffer_size);

#endif
