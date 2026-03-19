//🫖ketl
#ifndef ketl_compiler_assembler_builder_h
#define ketl_compiler_assembler_builder_h

#include "compiler/assembler.h"

#include "compiler/hir.h"

#include "containers/vector.h"
#include "containers/hash_map.h"

typedef uint8_t ketl_asm_x86_tag_t;
enum {
    KETL_ASM_LABEL,

    KETL_ASM_X86_MOV,
    KETL_ASM_X86_LEA,

    KETL_ASM_X86_ADD,
    KETL_ASM_X86_SUB,
    KETL_ASM_X86_IMUL,

    KETL_ASM_X86_CMP,
    KETL_ASM_X86_TEST,

    KETL_ASM_X86_SETE,
    KETL_ASM_X86_SETNE,

    KETL_ASM_X86_SETG,
    KETL_ASM_X86_SETGE,
    KETL_ASM_X86_SETL,
    KETL_ASM_X86_SETLE,

    KETL_ASM_X86_JMP,
    KETL_ASM_X86_JNE,

    KETL_ASM_X86_CALL,
    KETL_ASM_X86_RET,
};

typedef uint8_t ketl_asm_x86_size_t;
enum {
    KETL_ASM_X86_8B  = 1,
    KETL_ASM_X86_16B = 2,
    KETL_ASM_X86_32B = 4,
    KETL_ASM_X86_64B = 8,
    KETL_ASM_X86_PTRSIZE = sizeof(void*),
    KETL_ASM_X86_NONESIZE = 0,
};

typedef uint8_t ketl_asm_x86_arg_type_t;
enum {
    KETL_ASM_X86_EMPTY,
    KETL_ASM_X86_M,    // ModRM:r/m (r, w)
    KETL_ASM_X86_RM,   // ModRM:reg (w)       ModRM:r/m (r)
    KETL_ASM_X86_MR,   // ModRM:r/m (w)       ModRM:reg (r)
    KETL_ASM_X86_MI,   // ModRM:r/m (r, w)    imm
    KETL_ASM_X86_JI,   // .imm
};

typedef uint8_t ketl_asm_x86_reg_t;
enum {
    KETL_ASM_X86_AX,
    KETL_ASM_X86_CX,
    KETL_ASM_X86_DX,
    KETL_ASM_X86_BX,
    KETL_ASM_X86_SP,
    KETL_ASM_X86_BP,
    KETL_ASM_X86_SI,
    KETL_ASM_X86_DI,
    KETL_ASM_X86_R8,
    KETL_ASM_X86_R9,
    KETL_ASM_X86_R10,
    KETL_ASM_X86_R11,
    KETL_ASM_X86_R12,
    KETL_ASM_X86_R13,
    KETL_ASM_X86_R14,
    KETL_ASM_X86_R15,
    KETL_ASM_X86_REG_NONE,
};

ANN_DEFINE(ketl_asm_x86_modrm_t) {
    bool indir : 1;
    bool label : 1;
    ketl_asm_x86_reg_t base;
    ketl_asm_x86_reg_t index;
    ketl_asm_x86_size_t scale_power;
    union {
        int32_t disp;
        ketl_atomic_string s_literal;
    };
};

ANN_DEFINE(ketl_asm_x86_instr_t) {
    ketl_asm_x86_tag_t tag;
    ketl_asm_x86_size_t size;
    ketl_asm_x86_arg_type_t arg_type;
    uint8_t opcode_size;
    ketl_asm_x86_modrm_t modrm;
    union {
        ketl_asm_x86_reg_t reg;
        int8_t imm8;
        int16_t imm16;
        int32_t imm32;
        int64_t imm64;
        ketl_asm_x86_offset_t goto_offset;
    };
    uint64_t opcode_offset;
};

KETL_VECTOR_DECLARATION(ketl_asm_x86_instrs_t, ketl_asm_x86_instr_t)

KETL_HASH_MAP_DECLARATION(ketl_hir_to_asm_offsets_t, ketl_hir_instr_offset_t, ketl_asm_x86_offset_t)

ANN_FORWARD(ketl_type);

#define KETL_ASM_X86_ARG_PARENT_NONE ((ketl_hir_var_id_t)-1)

ANN_DEFINE(ketl_asm_x86_arg_info_t) {
    ketl_type* p_type;
    ketl_asm_x86_offset_t stack_offset;
};

KETL_HASH_MAP_DECLARATION(ketl_asm_x86_variables_t, ketl_hir_var_id_t, ketl_asm_x86_arg_info_t)

typedef uint8_t ketl_asm_x86_abi_type_t;
enum {
    KETL_ASM_X86_ABI_WINDOWS,
    KETL_ASM_X86_ABI_SYSTEM_V,
#if ANN_OS_WINDOWS
    KETL_ASM_X86_ABI_DEFAULT = KETL_ASM_X86_ABI_WINDOWS,
#else
    KETL_ASM_X86_ABI_DEFAULT = KETL_ASM_X86_ABI_SYSTEM_V,
#endif
};

ANN_DEFINE(ketl_asm_x86_builder_t) {
    ketl_state* p_state; 

    ketl_asm_x86_abi_type_t abi_type;
    bool inline_symbols;
    
    ketl_asm_x86_instrs_t instrs;
    ketl_hir_to_asm_offsets_t hir_to_asm_offsets;
    ketl_asm_x86_variables_t variables;
    ketl_hir_t* p_hir;
};

void ketl_asm_x86_builder_init(ketl_asm_x86_builder_t* p_builder, ketl_state* p_state, ketl_asm_x86_abi_type_t abi_type, bool inline_symbols);
void ketl_asm_x86_builder_deinit(ketl_asm_x86_builder_t* p_builder);

ANN_FORWARD(ketl_state);

void ketl_asm_x86_build(ketl_hir_t* p_hir, ketl_asm_x86_builder_t* p_builder, ketl_asm_x86_t* p_asm_x86, uint16_t func_index);
uint8_t* ketl_asm_x86_compile(ketl_asm_x86_t* p_asm_x86, uint32_t* p_opcodes_size, const ketl_allocator* p_allocator);

#endif
