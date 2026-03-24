//🫖ketl
#ifndef ketl_compiler_hir_h
#define ketl_compiler_hir_h

#include "type_impl.h"

#include "variable.h"
#include "token.h"

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

    KETL_HIR_NEW,
    KETL_HIR_CREATE_ARRAY,
    KETL_HIR_APPEND_VALUE,

    KETL_HIR_JUMP,

    KETL_HIR_JUMP_IF_TRUE,
    KETL_HIR_JUMP_IF_FALSE,

    KETL_HIR_RETURN,

    // type dependent instructions

#define KETL_HIR_TYPE_INSTR_MASK        0xFFF0
#define KETL_HIR_TYPE_INSTR_SHIFT       4

    // pre-shifted instructions

    __KETL_HIR_PLUS                     = 1,
    __KETL_HIR_MINUS,
    __KETL_HIR_MULTY,
    __KETL_HIR_DIV,
    __KETL_HIR_MOD,

    __KETL_HIR_EQUAL,
    __KETL_HIR_NOT_EQUAL,
    __KETL_HIR_LESS,
    __KETL_HIR_LESS_OR_EQUAL,
    __KETL_HIR_GREATER,
    __KETL_HIR_GREATER_OR_EQUAL,

    __KETL_HIR_CAST_TO_U8,
    __KETL_HIR_CAST_TO_U16,
    __KETL_HIR_CAST_TO_U32,
    __KETL_HIR_CAST_TO_U64,
    
    __KETL_HIR_CAST_TO_I8,
    __KETL_HIR_CAST_TO_I16,
    __KETL_HIR_CAST_TO_I32,
    __KETL_HIR_CAST_TO_I64,

    __KETL_HIR_ASSIGN,    
    
    __KETL_HIR_JUMP_IF_EQUAL,
    __KETL_HIR_JUMP_IF_NOT_EQAUL,
    
    __KETL_HIR_JUMP_IF_LESS,
    __KETL_HIR_JUMP_IF_LESS_OR_EQAUL,
    
    __KETL_HIR_JUMP_IF_GREATER,
    __KETL_HIR_JUMP_IF_GREATER_OR_EQAUL,

    __KETL_HIR_RETURN_VALUE,

    // shifted instructions

    KETL_HIR_PLUS                     = __KETL_HIR_PLUS << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_MINUS                    = __KETL_HIR_MINUS << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_MULTY                    = __KETL_HIR_MULTY << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_DIV                      = __KETL_HIR_DIV << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_MOD                      = __KETL_HIR_MOD << KETL_HIR_TYPE_INSTR_SHIFT,

    KETL_HIR_EQUAL                    = __KETL_HIR_EQUAL << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_NOT_EQUAL                = __KETL_HIR_NOT_EQUAL << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_LESS                     = __KETL_HIR_LESS << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_LESS_OR_EQUAL            = __KETL_HIR_LESS_OR_EQUAL << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_GREATER                  = __KETL_HIR_GREATER << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_GREATER_OR_EQUAL         = __KETL_HIR_GREATER_OR_EQUAL << KETL_HIR_TYPE_INSTR_SHIFT,

    KETL_HIR_CAST_TO_U8               = __KETL_HIR_CAST_TO_U8 << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_CAST_TO_U16              = __KETL_HIR_CAST_TO_U16 << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_CAST_TO_U32              = __KETL_HIR_CAST_TO_U32 << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_CAST_TO_U64              = __KETL_HIR_CAST_TO_U64 << KETL_HIR_TYPE_INSTR_SHIFT,
    
    KETL_HIR_CAST_TO_I8               = __KETL_HIR_CAST_TO_I8 << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_CAST_TO_I16              = __KETL_HIR_CAST_TO_I16 << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_CAST_TO_I32              = __KETL_HIR_CAST_TO_I32 << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_CAST_TO_I64              = __KETL_HIR_CAST_TO_I64 << KETL_HIR_TYPE_INSTR_SHIFT,

    KETL_HIR_ASSIGN                   = __KETL_HIR_ASSIGN << KETL_HIR_TYPE_INSTR_SHIFT,
    
    KETL_HIR_JUMP_IF_EQUAL            = __KETL_HIR_JUMP_IF_EQUAL << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_JUMP_IF_NOT_EQAUL        = __KETL_HIR_JUMP_IF_NOT_EQAUL << KETL_HIR_TYPE_INSTR_SHIFT,
    
    KETL_HIR_JUMP_IF_LESS             = __KETL_HIR_JUMP_IF_LESS << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_JUMP_IF_LESS_OR_EQAUL    = __KETL_HIR_JUMP_IF_LESS_OR_EQAUL << KETL_HIR_TYPE_INSTR_SHIFT,
    
    KETL_HIR_JUMP_IF_GREATER          = __KETL_HIR_JUMP_IF_GREATER << KETL_HIR_TYPE_INSTR_SHIFT,
    KETL_HIR_JUMP_IF_GREATER_OR_EQAUL = __KETL_HIR_JUMP_IF_GREATER_OR_EQAUL << KETL_HIR_TYPE_INSTR_SHIFT,

    KETL_HIR_RETURN_VALUE             = __KETL_HIR_RETURN_VALUE << KETL_HIR_TYPE_INSTR_SHIFT,

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
    ketl_hir_var_id_t a_arguments[];
};

ANN_DEFINE(ketl_hir_call_t) {
    ketl_hir_var_id_t output_var;
    ketl_hir_var_id_t callee;
    uint16_t arguments_count;
    ketl_hir_var_id_t a_arguments[];
};

ANN_DEFINE(ketl_hir_new_t) {
    ketl_hir_var_id_t output_var;
    ketl_hir_var_id_t type_var;
    uint16_t arguments_count;
    ketl_hir_var_id_t a_arguments[];
};

ANN_DEFINE(ketl_hir_create_array_t) {
    ketl_hir_var_id_t output_var;
    ketl_hir_var_id_t type_var;
    ketl_hir_var_id_t count_var_id;
};

ANN_DEFINE(ketl_hir_append_value_t) {
    ketl_hir_var_id_t array_var;
    ketl_hir_var_id_t append_var;
};

ANN_DEFINE(ketl_hir_cast_primitive_t) {
    ketl_hir_var_id_t dest_var;
    ketl_hir_var_id_t source_var;
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
#define KETL_HIR_VAR_UID_GLOBAL ((ketl_hir_var_uid_t)-2)
#define KETL_HIR_VAR_UID_FIELD ((ketl_hir_var_uid_t)-3)
#define KETL_HIR_VAR_UID_PARAMETER ((ketl_hir_var_uid_t)-4)
#define KETL_HIR_VAR_UID_INDEX ((ketl_hir_var_uid_t)-5)
#define KETL_HIR_VAR_UID_LAST KETL_HIR_VAR_UID_INDEX

#define KETL_HIR_LITERAL_NULL KETL_ATOMIC_STRING_EMPTY

#define KETL_HIR_USED_TYPE_UNKNOWN ((ketl_hir_used_type_index_t)-1)
#define KETL_HIR_USED_TYPE_LITERAL ((ketl_hir_used_type_index_t)-2)
#define KETL_HIR_USED_TYPE_META ((ketl_hir_used_type_index_t)-3)
#define KETL_HIR_USED_TYPE_LAST KETL_HIR_USED_TYPE_META

#define KETL_HIR_VAR_INFO_TEMP ((ketl_hir_var_info_index_t)-1)
#define KETL_HIR_VAR_NAME_TEMP KETL_ATOMIC_STRING_EMPTY

typedef uint32_t ketl_hir_expr_length_t;

ANN_DEFINE(ketl_hir_expr_info_t) {
    ketl_token_offset_t source_offset;
    ketl_hir_expr_length_t length;
};

ANN_DEFINE(ketl_hir_var_t) {
    union {
        ketl_hir_symbol_offset_t literal;
        ketl_hir_var_info_index_t info;
    };
    ketl_hir_used_type_index_t type;
    ketl_hir_var_uid_t uid;
    ketl_hir_expr_info_t expr_info;
};

ANN_FORWARD(ketl_namespace_node);

ANN_DEFINE(ketl_hir_var_info_t) {
    union {
        ketl_hir_symbol_offset_t name;
        ketl_hir_var_id_t arg_id;
    };
    union {
        ketl_namespace_node* p_global;
        ketl_hir_var_id_t parent_id;
    };
    ketl_hir_var_id_t last_var_id;
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
    ketl_hir_var_id_t parameter_count;
    bool has_calls;
};

void ketl_hir_deinit(ketl_hir_t* p_hir);

ketl_hir_instr_offset_t ketl_hir_get_instr_size(ketl_hir_tag_t tag, uint8_t* p_instr);

ketl_hir_instr_offset_t ketl_hir_decode_size(ketl_hir_t* p_hir, ketl_hir_instr_offset_t instr_offset);

ANN_FORWARD(ketl_state);

uint32_t ketl_hir_format(ketl_state* p_state, ketl_hir_t* p_hir, char* p_buffer, uint32_t buffer_size);

#endif
