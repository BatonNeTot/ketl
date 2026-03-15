//🫖ketl
#include "x86.h"

#include "ketl_impl.h"
#include "gc_memory.h"

#include "compiler/assembler_builder.h"

KETL_VECTOR_DECLARATION(opcodes_t, uint8_t)
KETL_VECTOR_DEFINITION(opcodes_t, uint8_t)

static void ketl_asm_x86_push_prefix(opcodes_t* p_opcodes, ketl_asm_x86_instr_t* p_instr) {
    if (p_instr->size == KETL_ASM_X86_16B) {
        opcodes_t_push_back_copy(p_opcodes, 0x66); // set 16-bit operand size
    }

    REXByte rex = {
        .prefix = REX_PREFIX,
        .w = 0, .r = 0, .x = 0, .b = 0};
    bool insert_rex = false;

    // TODO ommit when 64-bit size is implicit
    if (p_instr->size == KETL_ASM_X86_64B) {
        // set 64-bit operand size
        rex.w = 1;
        insert_rex = true;
    }
    
    if (p_instr->arg_type != KETL_ASM_X86_EMPTY && p_instr->arg_type != KETL_ASM_X86_JI) {
        if (p_instr->size == KETL_ASM_X86_8B &&
            ((p_instr->reg != KETL_ASM_X86_REG_NONE && p_instr->reg >= KETL_ASM_X86_R8) ||
            (p_instr->modrm.base != KETL_ASM_X86_REG_NONE && p_instr->modrm.base >= KETL_ASM_X86_R8))) {
            // for accessing spl, bpl, sil and dil rex must be inserted?
            // TODO check later, assert for now jic
            ANN_ASSERT(false);
        }
    
        if ((p_instr->arg_type == KETL_ASM_X86_RM || p_instr->arg_type == KETL_ASM_X86_MR) && p_instr->reg != KETL_ASM_X86_REG_NONE && p_instr->reg >= KETL_ASM_X86_R8) {
            // for accessing r8-r15 registers for reg
            rex.r = 1;
            insert_rex = true;
        }
        
        if (p_instr->modrm.base != KETL_ASM_X86_REG_NONE && p_instr->modrm.base >= KETL_ASM_X86_R8) {
            // for accessing r8-r15 registers for base
            rex.b = 1;
            insert_rex = true;
        }
        
        if (p_instr->modrm.index != KETL_ASM_X86_REG_NONE && p_instr->modrm.index >= KETL_ASM_X86_R8) {
            // for accessing r8-r15 registers for index
            rex.x = 1;
            insert_rex = true;
        }
    }

    if (insert_rex) {
        opcodes_t_push_back_ref(p_opcodes, (uint8_t*)(&rex));
    }
}

#define REX_AWARE_REG(reg) ((reg) & (KETL_ASM_X86_R8 - 1))

static void ketl_asm_x86_push_postfix_opcode(opcodes_t* p_opcodes, ketl_asm_x86_instr_t* p_instr, uint8_t reg) {
    ANN_SWITCH_STRICT (p_instr->arg_type) {
        case KETL_ASM_X86_EMPTY:
            break;
        case KETL_ASM_X86_M:
        case KETL_ASM_X86_RM:
        case KETL_ASM_X86_MR:
        case KETL_ASM_X86_MI: {
            MODRMByte modrm = {
                .mod = MODRM_MOD_DIR,
                .reg = reg,
                .rm = REX_AWARE_REG(p_instr->modrm.base),
            };

            if (!p_instr->modrm.indir) {
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&modrm);
                break;
            }

            if (p_instr->modrm.label) {
                modrm.rm = KETL_ASM_X86_BP;
                modrm.mod = MODRM_MOD_IND;
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&modrm);
                opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->modrm.disp, sizeof(p_instr->modrm.disp));
                break;
            }

            if (p_instr->modrm.index == KETL_ASM_X86_REG_NONE &&
                // https://wiki.osdev.org/X86-64_Instruction_Encoding#32/64-bit_addressing
                p_instr->modrm.base != KETL_ASM_X86_SP && p_instr->modrm.base != KETL_ASM_X86_R12) {
                if (p_instr->modrm.disp == 0) {
                    ANN_ASSERT(modrm.rm != KETL_ASM_X86_BP);
                    modrm.mod = MODRM_MOD_IND;
                    opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&modrm);
                } else if (p_instr->modrm.disp <= 127 && p_instr->modrm.disp >= -128) {
                    modrm.mod = MODRM_MOD_IND_DIS8;
                    opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&modrm);
                    int8_t disp = (int8_t)p_instr->modrm.disp;
                    opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&disp);
                } else {
                    modrm.mod = MODRM_MOD_IND_DIS32;
                    opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&modrm);
                    opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->modrm.disp, sizeof(p_instr->modrm.disp));
                }
                break;
            }

            // to enable SIB byte
            modrm.rm = KETL_ASM_X86_SP;
            ANN_ASSERT(p_instr->modrm.index != KETL_ASM_X86_SP);

            if (p_instr->modrm.base == KETL_ASM_X86_REG_NONE) {
                modrm.mod = MODRM_MOD_IND;
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&modrm);

                SIBByte sib = {
                    .scale =  p_instr->modrm.scale,
                    .index = p_instr->modrm.index == KETL_ASM_X86_REG_NONE ? KETL_ASM_X86_SP : REX_AWARE_REG(p_instr->modrm.index),
                    .base = KETL_ASM_X86_BP,
                };
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&sib);
                opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->modrm.disp, sizeof(p_instr->modrm.disp));
                break;
            }

            SIBByte sib = {
                .scale =  p_instr->modrm.scale,
                .index = p_instr->modrm.index == KETL_ASM_X86_REG_NONE ? KETL_ASM_X86_SP : REX_AWARE_REG(p_instr->modrm.index),
                .base = REX_AWARE_REG(p_instr->modrm.base),
            };

            if (p_instr->modrm.disp == 0 && 
                // https://wiki.osdev.org/X86-64_Instruction_Encoding#32/64-bit_addressing_2
                (p_instr->modrm.base != KETL_ASM_X86_BP && p_instr->modrm.base != KETL_ASM_X86_R13)) {
                modrm.mod = MODRM_MOD_IND;
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&modrm);
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&sib);
            } else if (p_instr->modrm.disp <= 127 && p_instr->modrm.disp >= -128) {
                modrm.mod = MODRM_MOD_IND_DIS8;
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&modrm);
                int8_t disp = (int8_t)p_instr->modrm.disp;
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&sib);
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&disp);
            } else {
                modrm.mod = MODRM_MOD_IND_DIS32;
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&modrm);
                opcodes_t_push_back_ref(p_opcodes, (uint8_t*)&sib);
                opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->modrm.disp, sizeof(p_instr->modrm.disp));
            }

            break;
        }

        case KETL_ASM_X86_JI:
            ANN_ASSERT(false);
            break;
    }
}

static void ketl_asm_x86_push_postfix(opcodes_t* p_opcodes, ketl_asm_x86_instr_t* p_instr) {
    ketl_asm_x86_push_postfix_opcode(p_opcodes, p_instr, REX_AWARE_REG(p_instr->reg));
}

static void ketl_asm_x86_push_opcode(opcodes_t* p_opcodes, ketl_asm_x86_instr_t* p_instrs, ketl_asm_x86_offset_t index) {
    ketl_asm_x86_instr_t* p_instr = &p_instrs[index];

    ANN_SWITCH_STRICT (p_instr->tag) {
        case KETL_ASM_LABEL: break;
        case KETL_ASM_X86_MOV: ///////////////////////////////////////////// 
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_RM: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0x8a : 0x8b);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
                case KETL_ASM_X86_MR: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0x88 : 0x89);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
                case KETL_ASM_X86_MI:
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0xb0 + REX_AWARE_REG(p_instr->modrm.base) : 0xb8 + REX_AWARE_REG(p_instr->modrm.base));
                    
                    ANN_SWITCH_STRICT (p_instr->size) {
                        case KETL_ASM_X86_8B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm8, sizeof(p_instr->imm8));
                            break;
                        case KETL_ASM_X86_16B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm16, sizeof(p_instr->imm16));
                            break;
                        case KETL_ASM_X86_32B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm32, sizeof(p_instr->imm32));
                            break;
                        case KETL_ASM_X86_64B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm64, sizeof(p_instr->imm64));
                            break;
                    }
                    break;
            }
            break;
        case KETL_ASM_X86_ADD: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_RM: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0x02 : 0x03);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
                case KETL_ASM_X86_MR: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0x00 : 0x01);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
                case KETL_ASM_X86_MI:
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0x80 : 0x81);
                    ketl_asm_x86_push_postfix_opcode(p_opcodes, p_instr, 0);
                    
                    ANN_SWITCH_STRICT (p_instr->size) {
                        case KETL_ASM_X86_8B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm8, sizeof(p_instr->imm8));
                            break;
                        case KETL_ASM_X86_16B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm16, sizeof(p_instr->imm16));
                            break;
                        case KETL_ASM_X86_32B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm32, sizeof(p_instr->imm32));
                            break;
                        case KETL_ASM_X86_64B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm32, sizeof(p_instr->imm32)); // imm here supports inly 32-bit
                            break;
                    }
                    break;
            }
            break;
        case KETL_ASM_X86_SUB: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_RM: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0x2a : 0x2b);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
                case KETL_ASM_X86_MR: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0x28 : 0x29);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
                case KETL_ASM_X86_MI:
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0x80 : 0x81);
                    ketl_asm_x86_push_postfix_opcode(p_opcodes, p_instr, 5);
                    
                    ANN_SWITCH_STRICT (p_instr->size) {
                        case KETL_ASM_X86_8B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm8, sizeof(p_instr->imm8));
                            break;
                        case KETL_ASM_X86_16B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm16, sizeof(p_instr->imm16));
                            break;
                        case KETL_ASM_X86_32B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm32, sizeof(p_instr->imm32));
                            break;
                        case KETL_ASM_X86_64B:
                            opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&p_instr->imm32, sizeof(p_instr->imm32)); // imm here supports inly 32-bit
                            break;
                    }
                    break;
            }
            break;
        case KETL_ASM_X86_IMUL: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_M: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0xf6 : 0xf7);
                    ketl_asm_x86_push_postfix_opcode(p_opcodes, p_instr, 5);
                    break;
            }
            break;
        case KETL_ASM_X86_CMP: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_RM: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0x3a : 0x3b);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
            }
            break;
        case KETL_ASM_X86_TEST: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_MR: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, p_instr->size == KETL_ASM_X86_8B ? 0x84 : 0x85);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
            }
            break;
        case KETL_ASM_X86_SETE: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_M: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, 0x0f);
                    opcodes_t_push_back_copy(p_opcodes, 0x94);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
            }
            break;
        case KETL_ASM_X86_SETNE: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_M: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, 0x0f);
                    opcodes_t_push_back_copy(p_opcodes, 0x95);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
            }
            break;
        case KETL_ASM_X86_SETG: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_M: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, 0x0f);
                    opcodes_t_push_back_copy(p_opcodes, 0x9f);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
            }
            break;
        case KETL_ASM_X86_SETGE: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_M: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, 0x0f);
                    opcodes_t_push_back_copy(p_opcodes, 0x9d);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
            }
            break;
        case KETL_ASM_X86_SETL: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_M: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, 0x0f);
                    opcodes_t_push_back_copy(p_opcodes, 0x9c);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
            }
            break;
        case KETL_ASM_X86_SETLE: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_M: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, 0x0f);
                    opcodes_t_push_back_copy(p_opcodes, 0x9e);
                    ketl_asm_x86_push_postfix(p_opcodes, p_instr);
                    break;
            }
            break;
        case KETL_ASM_X86_JMP: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_JI: 
                    opcodes_t_push_back_copy(p_opcodes, 0xe9);
                    int32_t diff = p_instrs[p_instr->goto_offset].opcode_offset - (p_instr->opcode_offset + p_instr->opcode_size);
                    opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&diff, sizeof(diff));
                    break;
            }
            break;
        case KETL_ASM_X86_JNE: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_JI: 
                    opcodes_t_push_back_copy(p_opcodes, 0x0f);
                    opcodes_t_push_back_copy(p_opcodes, 0x85);
                    int32_t diff = p_instrs[p_instr->goto_offset].opcode_offset - (p_instr->opcode_offset + p_instr->opcode_size);
                    opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&diff, sizeof(diff));
                    break;
            }
            break;
        case KETL_ASM_X86_CALL: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_M: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, 0xff);
                    ketl_asm_x86_push_postfix_opcode(p_opcodes, p_instr, 2);
                    break;
                case KETL_ASM_X86_JI: 
                    ketl_asm_x86_push_prefix(p_opcodes, p_instr);
                    opcodes_t_push_back_copy(p_opcodes, 0xe8);
                    int32_t diff = p_instrs[p_instr->goto_offset].opcode_offset - (p_instr->opcode_offset + p_instr->opcode_size);
                    opcodes_t_push_back_ref_n(p_opcodes, (uint8_t*)&diff, sizeof(diff));
                    break;
            }
            break;
        case KETL_ASM_X86_RET: ///////////////////////////////////////
            ANN_SWITCH_STRICT (p_instr->arg_type) {
                case KETL_ASM_X86_EMPTY: 
                    opcodes_t_push_back_copy(p_opcodes, 0xc3);
                    break;
            }
            break;
    }
}

/////////////////////////////////

#include "compiler/hir.h"

#include <stdio.h>
#include <stdlib.h>


KETL_VECTOR_DEFINITION(ketl_asm_x86_instrs_t, ketl_asm_x86_instr_t)
KETL_HASH_MAP_DEFINITION(ketl_hir_to_asm_offsets_t, ketl_hir_instr_offset_t, uint32_t, ANN_HASH, ANN_EQUAL)
KETL_HASH_MAP_DEFINITION(ketl_asm_x86_variables_t, ketl_hir_var_id_t, ketl_asm_x86_arg_info_t, ANN_HASH, ANN_EQUAL)

void ketl_asm_x86_builder_init(ketl_asm_x86_builder_t* p_builder, ketl_state* p_state, ketl_asm_x86_abi_type_t abi_type, bool inline_symbols) {
    p_builder->p_state = p_state;

    p_builder->abi_type = abi_type;
    p_builder->inline_symbols = inline_symbols;
    
    ketl_asm_x86_instrs_t_init(&p_builder->instrs, 8, p_state->p_allocator);
    ketl_hir_to_asm_offsets_t_init(&p_builder->hir_to_asm_offsets, p_state->p_allocator);
    ketl_asm_x86_variables_t_init(&p_builder->variables, p_state->p_allocator);
}
void ketl_asm_x86_builder_deinit(ketl_asm_x86_builder_t* p_builder) {
    ketl_asm_x86_variables_t_deinit(&p_builder->variables);
    ketl_hir_to_asm_offsets_t_deinit(&p_builder->hir_to_asm_offsets);
    ketl_asm_x86_instrs_t_deinit(&p_builder->instrs);
}

static void ketl_asm_x86_builder_reset(ketl_asm_x86_builder_t* p_builder) {
    p_builder->instrs.size = 0;
    ketl_hir_to_asm_offsets_t_clear(&p_builder->hir_to_asm_offsets);
    ketl_asm_x86_variables_t_clear(&p_builder->variables);
}

#define MODRM_EMPTY() ((ketl_asm_x86_modrm_t){\
    .indir = false,\
    .label = false,\
    .base = KETL_ASM_X86_REG_NONE,\
    .index = KETL_ASM_X86_REG_NONE,\
    .scale = 1,\
    .disp = 0,\
})

#define MODRM_LABEL(s_literal_) ((ketl_asm_x86_modrm_t){\
    .indir = false,\
    .label = true,\
    .base = KETL_ASM_X86_REG_NONE,\
    .index = KETL_ASM_X86_REG_NONE,\
    .scale = 1,\
    .s_literal = (s_literal_),\
})

#define MODRM_REG(reg) ((ketl_asm_x86_modrm_t){\
    .indir = false,\
    .label = false,\
    .base = reg,\
    .index = KETL_ASM_X86_REG_NONE,\
    .scale = 1,\
    .disp = 0,\
})

#define MODRM_INDIR_BASE_DISP(__base, __disp) ((ketl_asm_x86_modrm_t){\
    .indir = true,\
    .label = false,\
    .base = __base,\
    .index = KETL_ASM_X86_REG_NONE,\
    .scale = 1,\
    .disp = __disp,\
})

#define MODRM_INDIR_RIP_LABEL(__label) ((ketl_asm_x86_modrm_t){\
    .indir = true,\
    .label = true,\
    .base = KETL_ASM_X86_REG_NONE,\
    .index = KETL_ASM_X86_REG_NONE,\
    .scale = 1,\
    .s_literal = __label,\
})

static ketl_asm_x86_modrm_t modrm_stack(ketl_asm_x86_builder_t* p_builder, ketl_asm_x86_size_t size, int32_t stack_offset) {
    ANN_SWITCH_STRICT(p_builder->abi_type) {
        case KETL_ASM_X86_ABI_WINDOWS:
            return MODRM_INDIR_BASE_DISP(KETL_ASM_X86_SP, stack_offset);
        case KETL_ASM_X86_ABI_SYSTEM_V:
            return MODRM_INDIR_BASE_DISP(KETL_ASM_X86_BP, (0x100000000llu - stack_offset - size));
    }
}

static void ketl_asm_label(ketl_asm_x86_builder_t* p_builder, ketl_atomic_string s_label_name) {
    ketl_asm_x86_instr_t instr = {
        .tag = KETL_ASM_LABEL,
        .size = KETL_ASM_X86_NONESIZE, 
        .arg_type = KETL_ASM_X86_EMPTY,
        .modrm = MODRM_LABEL(s_label_name),
        .reg = KETL_ASM_X86_REG_NONE,
    };
    ketl_asm_x86_instrs_t_push_back_ref(&p_builder->instrs, &instr);
}

static void ketl_asm_x86_insert(ketl_asm_x86_builder_t* p_builder, ketl_asm_x86_tag_t tag) {
    ketl_asm_x86_instr_t instr = {
        .tag = tag,
        .size = KETL_ASM_X86_NONESIZE, 
        .arg_type = KETL_ASM_X86_EMPTY,
        .modrm = MODRM_EMPTY(),
        .reg = KETL_ASM_X86_REG_NONE,
    };
    ketl_asm_x86_instrs_t_push_back_ref(&p_builder->instrs, &instr);
}

static void ketl_asm_x86_insert_rm(ketl_asm_x86_builder_t* p_builder, ketl_asm_x86_tag_t tag, ketl_asm_x86_size_t size, ketl_asm_x86_modrm_t modrm) {
    ketl_asm_x86_instr_t instr = {
        .tag = tag,
        .size = size, 
        .arg_type = KETL_ASM_X86_M,
        .modrm = modrm,
        .reg = KETL_ASM_X86_REG_NONE,
    };
    ketl_asm_x86_instrs_t_push_back_ref(&p_builder->instrs, &instr);
}

static void ketl_asm_x86_insert_reg_rm(ketl_asm_x86_builder_t* p_builder, ketl_asm_x86_tag_t tag, ketl_asm_x86_size_t size, ketl_asm_x86_reg_t lhs, ketl_asm_x86_modrm_t rhs) {
    ketl_asm_x86_instr_t instr = {
        .tag = tag,
        .size = size, 
        .arg_type = KETL_ASM_X86_RM,
        .modrm = rhs,
        .reg = lhs,
    };
    ketl_asm_x86_instrs_t_push_back_ref(&p_builder->instrs, &instr);
}

static void ketl_asm_x86_insert_rm_reg(ketl_asm_x86_builder_t* p_builder, ketl_asm_x86_tag_t tag, ketl_asm_x86_size_t size, ketl_asm_x86_modrm_t lhs, ketl_asm_x86_reg_t rhs) {
    ketl_asm_x86_instr_t instr = {
        .tag = tag,
        .size = size, 
        .arg_type = KETL_ASM_X86_MR,
        .modrm = lhs,
        .reg = rhs,
    };
    ketl_asm_x86_instrs_t_push_back_ref(&p_builder->instrs, &instr);
}

static void ketl_asm_x86_insert_rm_imm(ketl_asm_x86_builder_t* p_builder, ketl_asm_x86_tag_t tag, ketl_asm_x86_size_t size, ketl_asm_x86_modrm_t lhs, uint64_t rhs) {
    ketl_asm_x86_instr_t instr = {
        .tag = tag,
        .size = size, 
        .arg_type = KETL_ASM_X86_MI,
        .modrm = lhs,
        .imm64 = rhs,
    };
    ketl_asm_x86_instrs_t_push_back_ref(&p_builder->instrs, &instr);
}
static void ketl_asm_x86_insert_jimm(ketl_asm_x86_builder_t* p_builder, ketl_asm_x86_tag_t tag, ketl_asm_x86_size_t size, ketl_asm_x86_offset_t val) {
    ketl_asm_x86_instr_t instr = {
        .tag = tag,
        .size = size, 
        .arg_type = KETL_ASM_X86_JI,
        .modrm = MODRM_EMPTY(),
        .goto_offset = val,
    };
    ketl_asm_x86_instrs_t_push_back_ref(&p_builder->instrs, &instr);
}

static ketl_hir_block_index_t get_first_non_empty_block(ketl_hir_t* p_hir, ketl_hir_block_index_t block) {
    ANN_FOREVER {
        ketl_hir_instr_offset_t instr_offset = p_hir->p_block_offsets[block];
        uint8_t* p_instr = p_hir->p_instrs + instr_offset;
        ketl_hir_header_t* p_header = (ketl_hir_header_t*)p_instr;
        if (p_header->tag != KETL_HIR_JUMP) {
            return block;
        }
        ketl_hir_jump_t* p_jump = (ketl_hir_jump_t*)(p_instr + sizeof(ketl_hir_header_t));
        block = p_jump->block_index;
    }
}

static ketl_asm_x86_size_t get_size_from_hir_type(ketl_hir_tag_t tag) {
    ANN_SWITCH_STRICT (tag & KETL_HIR_TYPE_MASK) {
        case KETL_HIR_U8:
        case KETL_HIR_I8:
            return 1;
        case KETL_HIR_U16:
        case KETL_HIR_I16:
            return 2;
        case KETL_HIR_U32:
        case KETL_HIR_I32:
        case KETL_HIR_F32:
            return 4;
        case KETL_HIR_U64:
        case KETL_HIR_I64:
        case KETL_HIR_F64:
            return 8;
    }
}

static void ketl_asm_x86_bake_offsets(ketl_asm_x86_instr_t* p_instrs, ketl_asm_x86_offset_t size, const ketl_allocator* p_allocator) {
    opcodes_t opcodes;
    opcodes_t_init(&opcodes, 16, p_allocator);

    uint64_t total_size = 0;

    for (uint32_t i = 0u; i < size; ++i) {
        p_instrs[i].opcode_offset = total_size;
        ketl_asm_x86_push_opcode(&opcodes, p_instrs, i);
        total_size += (p_instrs[i].opcode_size = opcodes.size);
        opcodes.size = 0;
    }

    opcodes_t_deinit(&opcodes);
}

static ketl_asm_x86_reg_t get_reg_parameter(ketl_asm_x86_abi_type_t abi_type, uint32_t parameter_index) {
    ANN_SWITCH_STRICT (abi_type) {
        case KETL_ASM_X86_ABI_WINDOWS: {
            static ketl_asm_x86_reg_t a_parameter_regs[] = {
                KETL_ASM_X86_CX,
                KETL_ASM_X86_DX,
                KETL_ASM_X86_R8,
                KETL_ASM_X86_R9,
            };

            if (parameter_index >= ANN_ARRAY_SIZE(a_parameter_regs)) {
                ANN_ASSERT(false);
            }
            return a_parameter_regs[parameter_index];
        }
        case KETL_ASM_X86_ABI_SYSTEM_V: {
            static ketl_asm_x86_reg_t a_parameter_regs[] = {
                KETL_ASM_X86_DI,
                KETL_ASM_X86_SI,
                KETL_ASM_X86_DX,
                KETL_ASM_X86_CX,
                KETL_ASM_X86_R8,
                KETL_ASM_X86_R9,
            };

            if (parameter_index >= ANN_ARRAY_SIZE(a_parameter_regs)) {
                ANN_ASSERT(false);
            }
            return a_parameter_regs[parameter_index];
        }
    }
} 

static bool try_adapt_stask_size(ketl_asm_x86_abi_type_t abi_type, ketl_asm_x86_offset_t* p_stack_usage, bool has_calls) {
    ketl_asm_x86_offset_t stack_usage = *p_stack_usage;

    ANN_SWITCH_STRICT (abi_type) {
        case KETL_ASM_X86_ABI_WINDOWS: {
#define WINDOWS_SHADOW_STACK_SPACE 8 // I'm not sure about it, I came to this during msvc disasm study
            if (!has_calls && stack_usage <= 0) {
                return false;
            }

            if (has_calls) {
                stack_usage += (8 * 4); // size of reg parameters, has to be preallocated
            }
            stack_usage = ANN_ALIGN_FORWARD(stack_usage, 16); // 16-bites aligned
            stack_usage += WINDOWS_SHADOW_STACK_SPACE; // add shadow space AFTER alignment
            *p_stack_usage = stack_usage;
            return true;
        }
        case KETL_ASM_X86_ABI_SYSTEM_V: {
#define SYSTEM_V_SHADOW_STACK_SPACE 128
            if (!((has_calls && stack_usage > 0) || (!has_calls && stack_usage > SYSTEM_V_SHADOW_STACK_SPACE))) {
                return false;
            }

            if (!has_calls && stack_usage > SYSTEM_V_SHADOW_STACK_SPACE) {
                stack_usage -= SYSTEM_V_SHADOW_STACK_SPACE;
            }
            stack_usage = ANN_ALIGN_FORWARD(stack_usage, 16); // 16-bites aligned

            return true;
        }
    };
}

static void push_mov_from_stack(ketl_asm_x86_reg_t target_reg, ketl_hir_var_id_t var_id, ketl_asm_x86_size_t size, ketl_asm_x86_builder_t* p_builder) {
    ketl_hir_var_t var = p_builder->p_hir->p_vars[var_id];

    if (var.uid == KETL_HIR_VAR_UID_LITERAL) {
        const char* p_literal = KETL_ATOMIC_STRING_GET_POINTER(p_builder->p_hir->p_symbols, var.literal);

        int64_t value = p_literal != NULL ? strtoll(p_literal, NULL, 10) : 0;
        ketl_type* p_type = p_builder->p_hir->p_used_types[var.type];
        (void)p_type;

        // TODO FIX
        // get type size and use appropriate load global bytecode
        ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_64B, MODRM_REG(target_reg), value);
        return;
    }

    if (var.info != KETL_HIR_VAR_INFO_TEMP && var.uid == KETL_HIR_VAR_UID_GLOBAL) {
        ANN_ASSERT(var.type != KETL_HIR_USED_TYPE_META);

        ketl_namespace_node* p_namespace_node = p_builder->p_hir->p_vars_infos[var.info].p_global;

        if (p_builder->inline_symbols) {
            ketl_atomic_string s_name = p_namespace_node->s_name;
            if (!p_namespace_node->export) {
                char a_buffer[256];
                uint32_t length = snprintf(a_buffer, ANN_ARRAY_SIZE(a_buffer), ".%s", ketl_atomic_strings_get_pointer(&p_builder->p_state->atomic_strings, s_name));
                s_name = ketl_atomic_strings_get(&p_builder->p_state->atomic_strings, a_buffer, length);
            }
            ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_64B, target_reg, MODRM_INDIR_RIP_LABEL(s_name));
        } else {
            // TODO FIX
            // get type size and use appropriate load global bytecode
            ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_64B, MODRM_REG(target_reg), (int64_t)&p_namespace_node->variable);
            ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_64B, target_reg, MODRM_INDIR_BASE_DISP(target_reg, 0));
        }
        return;
    }

    if (var.info != KETL_HIR_VAR_INFO_TEMP && var.uid == KETL_HIR_VAR_UID_FIELD) {
        ketl_hir_var_id_t object = p_builder->p_hir->p_vars_infos[var.info].parent_id;

        ketl_asm_x86_variables_t_bucket* object_bucket = ketl_asm_x86_variables_t_get_or_null(&p_builder->variables, object);
        ANN_ASSERT(object_bucket != NULL);

        ketl_asm_x86_reg_t donor_reg = KETL_ASM_X86_AX;
        // TODO get proper size
        push_mov_from_stack(donor_reg, object, size, p_builder);
        ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_MOV, size, target_reg, MODRM_INDIR_BASE_DISP(donor_reg, object_bucket->value.stack_offset)); 
        return;
    }

    if (var.info != KETL_HIR_VAR_INFO_TEMP && var.uid == KETL_HIR_VAR_UID_INDEX) {
        ketl_hir_var_id_t object_id = p_builder->p_hir->p_vars_infos[var.info].parent_id;
        ketl_hir_var_id_t arg_id = p_builder->p_hir->p_vars_infos[var.info].arg_id;

        ketl_hir_var_t object = p_builder->p_hir->p_vars[object_id];
        ANN_ASSERT(object.type != KETL_HIR_USED_TYPE_UNKNOWN);
        ketl_type* p_object_type = p_builder->p_hir->p_used_types[object.type];

        ketl_asm_x86_reg_t donor_arg_reg = KETL_ASM_X86_AX;
        ketl_asm_x86_reg_t donor_object_reg = KETL_ASM_X86_DX;

        // get argument into rax
        push_mov_from_stack(donor_arg_reg, arg_id, size, p_builder);
        // get type size into rcx
        ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, MODRM_REG(donor_object_reg), ketl_type_get_stack_size(p_object_type));
        // multiply, result in rax
        // TODO use unsigned multiply
        ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_IMUL, KETL_ASM_X86_PTRSIZE, MODRM_REG(donor_object_reg));

        // get array
        // TODO get proper size
        push_mov_from_stack(donor_object_reg, object_id, size, p_builder);
        ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_64B, donor_object_reg, MODRM_INDIR_BASE_DISP(donor_object_reg, 0));

        // add multiplication result, result in rax
        ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_ADD, size, donor_arg_reg, MODRM_REG(donor_object_reg));

        // indirect read 
        ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_MOV, size, target_reg, MODRM_INDIR_BASE_DISP(donor_arg_reg, 0)); 
        return;
    }

    ketl_asm_x86_variables_t_bucket* bucket = ketl_asm_x86_variables_t_get_or_null(&p_builder->variables, var_id);
    ANN_ASSERT(bucket != NULL);

    ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_MOV, size, target_reg, modrm_stack(p_builder, size, bucket->value.stack_offset)); 
}

static void push_mov_to_stack(ketl_hir_var_id_t var_id, ketl_asm_x86_reg_t source_reg, ketl_asm_x86_size_t size, ketl_asm_x86_builder_t* p_builder) {
    ketl_hir_var_t var = p_builder->p_hir->p_vars[var_id];
    ANN_ASSERT(var.uid != KETL_HIR_VAR_UID_LITERAL);

    if (var.info != KETL_HIR_VAR_INFO_TEMP && var.uid == KETL_HIR_VAR_UID_GLOBAL) {
        ANN_ASSERT(var.type != KETL_HIR_USED_TYPE_META);

        ketl_namespace_node* p_namespace_node = p_builder->p_hir->p_vars_infos[var.info].p_global;

        if (p_builder->inline_symbols) {
            ketl_atomic_string s_name = p_namespace_node->s_name;
            if (!p_namespace_node->export) {
                char a_buffer[256];
                uint32_t length = snprintf(a_buffer, ANN_ARRAY_SIZE(a_buffer), ".%s", ketl_atomic_strings_get_pointer(&p_builder->p_state->atomic_strings, s_name));
                s_name = ketl_atomic_strings_get(&p_builder->p_state->atomic_strings, a_buffer, length);
            }
            ketl_asm_x86_insert_rm_reg(p_builder, KETL_ASM_X86_MOV, size, MODRM_INDIR_RIP_LABEL(s_name), source_reg); 
        } else {
            ketl_asm_x86_reg_t donor_reg = source_reg != KETL_ASM_X86_AX ? KETL_ASM_X86_AX : KETL_ASM_X86_CX;
            // TODO get proper size
            ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_64B, MODRM_REG(donor_reg), (int64_t)&p_namespace_node->variable);
            ketl_asm_x86_insert_rm_reg(p_builder, KETL_ASM_X86_MOV, size, MODRM_INDIR_BASE_DISP(donor_reg, 0), source_reg); 
        }
        return;
    }

    if (var.info != KETL_HIR_VAR_INFO_TEMP && var.uid == KETL_HIR_VAR_UID_FIELD) {
        ketl_hir_var_id_t object = p_builder->p_hir->p_vars_infos[var.info].parent_id;

        ketl_asm_x86_variables_t_bucket* object_bucket = ketl_asm_x86_variables_t_get_or_null(&p_builder->variables, object);
        ANN_ASSERT(object_bucket != NULL);
        
        ketl_asm_x86_reg_t donor_reg = source_reg != KETL_ASM_X86_AX ? KETL_ASM_X86_AX : KETL_ASM_X86_CX;
        // TODO get proper size
        push_mov_from_stack(donor_reg, object, size, p_builder);
        ketl_asm_x86_insert_rm_reg(p_builder, KETL_ASM_X86_MOV, size, MODRM_INDIR_BASE_DISP(donor_reg, object_bucket->value.stack_offset), source_reg); 
        return;
    }

    if (var.info != KETL_HIR_VAR_INFO_TEMP && var.uid == KETL_HIR_VAR_UID_INDEX) {
        ketl_hir_var_id_t object_id = p_builder->p_hir->p_vars_infos[var.info].parent_id;
        ketl_hir_var_id_t arg_id = p_builder->p_hir->p_vars_infos[var.info].arg_id;

        ketl_hir_var_t object = p_builder->p_hir->p_vars[object_id];
        ANN_ASSERT(object.type != KETL_HIR_USED_TYPE_UNKNOWN);
        ketl_type* p_object_type = p_builder->p_hir->p_used_types[object.type];

        ketl_asm_x86_reg_t donor_arg_reg = KETL_ASM_X86_AX;
        ketl_asm_x86_reg_t donor_object_reg = KETL_ASM_X86_DX;

        ketl_asm_x86_reg_t save_reg = KETL_ASM_X86_R8;

        // save value from source reg
        ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_MOV, size, save_reg, MODRM_REG(source_reg));

        // get argument into rax
        push_mov_from_stack(donor_arg_reg, arg_id, size, p_builder);
        // get type size into rcx
        ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, MODRM_REG(donor_object_reg), ketl_type_get_stack_size(p_object_type));
        // multiply, result in rax
        // TODO use unsigned multiply
        ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_IMUL, KETL_ASM_X86_PTRSIZE, MODRM_REG(donor_object_reg));

        // get array
        // TODO get proper size
        push_mov_from_stack(donor_object_reg, object_id, size, p_builder);
        ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_64B, donor_object_reg, MODRM_INDIR_BASE_DISP(donor_object_reg, 0));

        // add multiplication result, result in rax
        ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_ADD, size, donor_arg_reg, MODRM_REG(donor_object_reg));

        // indirect write 
        ketl_asm_x86_insert_rm_reg(p_builder, KETL_ASM_X86_MOV, size, MODRM_INDIR_BASE_DISP(donor_arg_reg, 0), save_reg);
        return;
    }

    ketl_asm_x86_variables_t_bucket* bucket = ketl_asm_x86_variables_t_get_or_null(&p_builder->variables, var_id);
    ANN_ASSERT(bucket != NULL);

    ketl_asm_x86_insert_rm_reg(p_builder, KETL_ASM_X86_MOV, size, modrm_stack(p_builder, size, bucket->value.stack_offset), source_reg);     
}

void ketl_asm_x86_build(ketl_hir_t* p_hir, ketl_asm_x86_builder_t* p_builder, ketl_asm_x86_t* p_asm_x86, uint16_t func_index) {
    ketl_asm_x86_builder_reset(p_builder);

    p_builder->p_hir = p_hir;
    
    ketl_asm_x86_offset_t stack_reserved_size = 0u;

#if ANN_OS_WINDOWS
    for (ketl_hir_var_id_t var_id = p_hir->parameter_count; var_id > 0; ) {
        --var_id;
        ketl_hir_var_t var = p_hir->p_vars[var_id];

        ANN_ASSERT(var.uid == KETL_HIR_VAR_UID_PARAMETER);

        // local or temporary variable
        ketl_asm_x86_arg_info_t arg_info = {
            .p_type = var.type != KETL_HIR_USED_TYPE_UNKNOWN ? p_hir->p_used_types[var.type] : NULL,
            .stack_offset = (var_id + 1) * sizeof(void*),
        };

        ketl_asm_x86_variables_t_get_or_insert_copy(&p_builder->variables, var_id, arg_info);

        ketl_asm_x86_insert_rm_reg(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_64B, modrm_stack(p_builder, KETL_ASM_X86_64B, arg_info.stack_offset), 
            get_reg_parameter(p_builder->abi_type, var_id));
    }

    int32_t stack_reserved_size_offset = p_builder->instrs.size;
    ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_SUB, KETL_ASM_X86_PTRSIZE, MODRM_REG(KETL_ASM_X86_SP), 0);
#else
    // TODO
    ANN_ASSERT(false);
#endif

    for (ketl_hir_var_id_t var_id = p_hir->parameter_count; var_id < p_hir->vars_count; ++var_id) {
        ketl_hir_var_t var = p_hir->p_vars[var_id];

        ANN_ASSERT(var.uid != KETL_HIR_VAR_UID_PARAMETER);
        if (var.uid == KETL_HIR_VAR_UID_LITERAL ||
            var.uid == KETL_HIR_VAR_UID_GLOBAL ||
            var.uid == KETL_HIR_VAR_UID_INDEX) {
            continue;
        }

        if (var.info != KETL_HIR_VAR_INFO_TEMP && var.uid == KETL_HIR_VAR_UID_FIELD) {
            ketl_hir_var_id_t object = p_hir->p_vars_infos[var.info].parent_id;

            ketl_asm_x86_variables_t_bucket* object_bucket = ketl_asm_x86_variables_t_get_or_null(&p_builder->variables, object);
            ANN_ASSERT(object_bucket != NULL);

            const char* p_field_name = KETL_ATOMIC_STRING_GET_POINTER(p_hir->p_symbols, p_hir->p_vars_infos[var.info].name);

            ketl_atomic_string s_field_name = ketl_atomic_strings_get(&p_builder->p_state->atomic_strings, p_field_name, KETL_NULL_TERMINATED_LENGTH_32);
            ketl_asm_x86_offset_t field_offset = ketl_type_get_class_field_offset(object_bucket->value.p_type, s_field_name);

            ketl_asm_x86_arg_info_t arg_info = {
                .p_type = var.type != KETL_HIR_USED_TYPE_UNKNOWN ? p_hir->p_used_types[var.type] : NULL,
                .stack_offset = field_offset,
            };

            ketl_asm_x86_variables_t_get_or_insert_copy(&p_builder->variables, var_id, arg_info);
        }

        // local or temporary variable
        ketl_asm_x86_arg_info_t arg_info = {
            .p_type = var.type != KETL_HIR_USED_TYPE_UNKNOWN ? p_hir->p_used_types[var.type] : NULL,
            .stack_offset = stack_reserved_size,
        };

        ketl_asm_x86_variables_t_bucket* p_bucket = ketl_asm_x86_variables_t_get_or_insert_copy(&p_builder->variables, var_id, arg_info);
        if (p_bucket->value.stack_offset == arg_info.stack_offset) {
            // TODO FIX take into account type size and alignment
            stack_reserved_size += sizeof(int64_t);
        }
    }
    
    if (!try_adapt_stask_size(p_builder->abi_type, &stack_reserved_size, p_hir->has_calls)) {
        stack_reserved_size = 0u;
    }
    p_builder->instrs.p_data[stack_reserved_size_offset].imm32 = (int32_t)stack_reserved_size;

    
    for (ketl_hir_var_id_t var_id = p_hir->parameter_count; var_id > 0; ) {
        --var_id;

        ketl_asm_x86_variables_t_bucket* p_bucket = ketl_asm_x86_variables_t_get_or_null(&p_builder->variables, var_id);

        p_bucket->value.stack_offset += stack_reserved_size;
    }

    bool first_instr_in_block = true;
    uint16_t block_counter = 0;

    for (uint32_t i = 0u; i < p_hir->instrs_count; i += ketl_hir_decode_size(p_hir, i)) {
        if (first_instr_in_block) {
            first_instr_in_block = false;
            ketl_hir_to_asm_offsets_t_get_or_insert_copy(&p_builder->hir_to_asm_offsets, i, p_builder->instrs.size);

            char a_label_buffer[256] = {'\0'};
            uint32_t label_length = snprintf(a_label_buffer, ANN_ARRAY_SIZE(a_label_buffer), ".LBB%"PRIu16"_%"PRIu16, func_index, block_counter);
            ketl_atomic_string s_label = ketl_atomic_strings_get(&p_builder->p_state->atomic_strings, a_label_buffer, label_length);
            ++block_counter;

            ketl_asm_label(p_builder, s_label);
        }

        uint8_t* p_instr = p_hir->p_instrs + i;

        ketl_hir_header_t header = *(ketl_hir_header_t*)p_instr;
        p_instr += sizeof(ketl_hir_header_t);

        switch (header.tag & KETL_HIR_TYPE_INSTR_MASK) {
            case KETL_HIR_PLUS: {
                ketl_hir_binary_op_t* p_hir_info = (ketl_hir_binary_op_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_CX, p_hir_info->rhs_var, size, p_builder);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->lhs_var, size, p_builder);
                ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_ADD, size, KETL_ASM_X86_AX, MODRM_REG(KETL_ASM_X86_CX));
                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, size, p_builder);
                continue;
            }
            case KETL_HIR_MINUS: {
                ketl_hir_binary_op_t* p_hir_info = (ketl_hir_binary_op_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_CX, p_hir_info->rhs_var, size, p_builder);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->lhs_var, size, p_builder);
                ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_SUB, size, KETL_ASM_X86_AX, MODRM_REG(KETL_ASM_X86_CX));
                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, size, p_builder);
                continue;
            }
            case KETL_HIR_MULTY: {
                ketl_hir_binary_op_t* p_hir_info = (ketl_hir_binary_op_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_CX, p_hir_info->rhs_var, size, p_builder);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->lhs_var, size, p_builder);
                ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_IMUL, size, MODRM_REG(KETL_ASM_X86_CX));
                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, size, p_builder);
                continue;
            }
            case KETL_HIR_DIV:
            case KETL_HIR_MOD:
                ANN_ASSERT(false);
                continue;

            case KETL_HIR_EQUAL: {
                ketl_hir_binary_op_t* p_hir_info = (ketl_hir_binary_op_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_CX, p_hir_info->rhs_var, size, p_builder);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->lhs_var, size, p_builder);
                ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_CMP, size, KETL_ASM_X86_AX, MODRM_REG(KETL_ASM_X86_CX));
                ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_SETE, KETL_ASM_X86_8B, MODRM_REG(KETL_ASM_X86_AX));
                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, size, p_builder);
                continue;
            }
            case KETL_HIR_NOT_EQUAL: {
                ketl_hir_binary_op_t* p_hir_info = (ketl_hir_binary_op_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_CX, p_hir_info->rhs_var, size, p_builder);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->lhs_var, size, p_builder);
                ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_CMP, size, KETL_ASM_X86_AX, MODRM_REG(KETL_ASM_X86_CX));
                ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_SETNE, KETL_ASM_X86_8B, MODRM_REG(KETL_ASM_X86_AX));
                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, size, p_builder);
                continue;
            }
            case KETL_HIR_LESS: {
                ketl_hir_binary_op_t* p_hir_info = (ketl_hir_binary_op_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_CX, p_hir_info->rhs_var, size, p_builder);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->lhs_var, size, p_builder);
                ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_CMP, size, KETL_ASM_X86_AX, MODRM_REG(KETL_ASM_X86_CX));
                ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_SETL, KETL_ASM_X86_8B, MODRM_REG(KETL_ASM_X86_AX));
                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, size, p_builder);
                continue;
            }
            case KETL_HIR_LESS_OR_EQUAL: {
                ketl_hir_binary_op_t* p_hir_info = (ketl_hir_binary_op_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_CX, p_hir_info->rhs_var, size, p_builder);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->lhs_var, size, p_builder);
                ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_CMP, size, KETL_ASM_X86_AX, MODRM_REG(KETL_ASM_X86_CX));
                ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_SETLE, KETL_ASM_X86_8B, MODRM_REG(KETL_ASM_X86_AX));
                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, size, p_builder);
                continue;
            }
            case KETL_HIR_GREATER: {
                ketl_hir_binary_op_t* p_hir_info = (ketl_hir_binary_op_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_CX, p_hir_info->rhs_var, size, p_builder);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->lhs_var, size, p_builder);
                ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_CMP, size, KETL_ASM_X86_AX, MODRM_REG(KETL_ASM_X86_CX));
                ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_SETG, KETL_ASM_X86_8B, MODRM_REG(KETL_ASM_X86_AX));
                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, size, p_builder);
                continue;
            }
            case KETL_HIR_GREATER_OR_EQUAL: {
                ketl_hir_binary_op_t* p_hir_info = (ketl_hir_binary_op_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_CX, p_hir_info->rhs_var, size, p_builder);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->lhs_var, size, p_builder);
                ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_CMP, size, KETL_ASM_X86_AX, MODRM_REG(KETL_ASM_X86_CX));
                ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_SETGE, KETL_ASM_X86_8B, MODRM_REG(KETL_ASM_X86_AX));
                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, size, p_builder);
                continue;
            }

            case KETL_HIR_ASSIGN: {
                ketl_hir_assign_t* p_hir_info = (ketl_hir_assign_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->source_var, size, p_builder);
                push_mov_to_stack(p_hir_info->dest_var, KETL_ASM_X86_AX, size, p_builder);
                continue;
            }
            
            case KETL_HIR_RETURN_VALUE: {
                ketl_hir_return_value_t* p_hir_info = (ketl_hir_return_value_t*)p_instr;
                ketl_asm_x86_size_t size = get_size_from_hir_type(header.tag);
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->value_var, size, p_builder);
                ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_ADD, KETL_ASM_X86_PTRSIZE, MODRM_REG(KETL_ASM_X86_SP), stack_reserved_size);
                ketl_asm_x86_insert(p_builder, KETL_ASM_X86_RET);

                first_instr_in_block = true;
                continue;
            }
        }

        ANN_SWITCH_STRICT (header.tag) {
            case KETL_HIR_CALL: {
                ketl_hir_call_t* p_hir_info = (ketl_hir_call_t*)p_instr;

                ketl_hir_var_t callee = p_hir->p_vars[p_hir_info->callee];
                ANN_ASSERT(callee.type != KETL_HIR_USED_TYPE_UNKNOWN);
                ketl_type* p_callee_type = p_hir->p_used_types[callee.type];

                // TODO FIX allow other types to be called
                ANN_ASSERT(p_callee_type->kind == KETL_TYPE_FUNCTION || p_callee_type->kind == KETL_TYPE_CFUNCTION);

                // push arguments
                for (uint32_t i = p_hir_info->arguments_count - 1; i != (uint32_t) -1; --i) {
                    // TODO FIX
                    // get type size and use appropriate
                    ketl_asm_x86_size_t size = KETL_ASM_X86_64B;
                    push_mov_from_stack(get_reg_parameter(p_builder->abi_type, i), p_hir_info->arguments[i], size, p_builder);
                }

                // call
                if (p_builder->inline_symbols) {
                    ketl_namespace_node* p_namespace_node = p_builder->p_hir->p_vars_infos[callee.info].p_global;
                    ketl_atomic_string s_func_name = p_namespace_node->variable.p_func->s_name;
                    if (!p_namespace_node->export) {
                        char a_buffer[256];
                        uint32_t length = snprintf(a_buffer, ANN_ARRAY_SIZE(a_buffer), ".%s", ketl_atomic_strings_get_pointer(&p_builder->p_state->atomic_strings, s_func_name));
                        s_func_name = ketl_atomic_strings_get(&p_builder->p_state->atomic_strings, a_buffer, length);
                    }
                    ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_CALL, KETL_ASM_X86_PTRSIZE, MODRM_LABEL(s_func_name));
                } else {
                    push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->callee, KETL_ASM_X86_PTRSIZE, p_builder);
                    ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_CALL, KETL_ASM_X86_PTRSIZE, MODRM_REG(KETL_ASM_X86_AX));
                }
                    
                // TODO get correct size
                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, KETL_ASM_X86_64B, p_builder);
                continue;
            }
            case KETL_HIR_CREATE: {
                ketl_hir_create_t* p_hir_info = (ketl_hir_create_t*)p_instr;

                //ketl_gc_create(&p_builder->p_state->gc, p_hir->p_used_types[p_hir_info->type], 0);
                
                ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, MODRM_REG(get_reg_parameter(p_builder->abi_type, 2)), 0); // flags
                ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, MODRM_REG(get_reg_parameter(p_builder->abi_type, 1)), (uint64_t)p_hir->p_used_types[p_hir_info->type]);
                ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, MODRM_REG(get_reg_parameter(p_builder->abi_type, 0)), (uint64_t)&p_builder->p_state->gc);

                void* func_address;
                #define KETL_POINTER_CONVERTER
                #define KETL_POINTER_CONVERTER_ARG  &ketl_gc_create
                #define KETL_POINTER_CONVERTER_VAR  func_address
                #define KETL_POINTER_CONVERTER_TYPE void*
                #include "meta.i"
                 
                ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, MODRM_REG(KETL_ASM_X86_AX), (uint64_t)func_address);

                // call create from gc
                ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_CALL, KETL_ASM_X86_PTRSIZE, MODRM_REG(KETL_ASM_X86_AX));

                // TODO call constructor

                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, KETL_ASM_X86_PTRSIZE, p_builder);

                // ANN_ASSERT(false);
                // ketl_asm_x86_arg_info_t callee_arg = hir_get_arg_stack_offset(p_builder, p_hir_info->type); // TODO wrong!

                // // TODO FIX allow other types to be called
                // ANN_ASSERT(callee_arg.p_type->type == KETL_TYPE_CFUNCTION);

                // for (uint32_t i = p_hir_info->arguments_count - 1; i != (uint32_t) -1; --i) {
                //     ketl_asm_x86_arg_info_t arg = hir_get_arg_stack_offset(p_builder, p_hir_info->arguments[i]);

                //     if (i >= ANN_ARRAY_SIZE(a_parameter_regs)) {
                //         ANN_ASSERT(false);
                //     }
                //     // TODO FIX
                //     // get type size and use appropriate
                //     ketl_asm_x86_size_t size = KETL_ASM_X86_64B;
                //     ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_MOV, size, 
                //         a_parameter_regs[i], modrm_stack(p_builder, size, arg.stack_offset));
                // }

                // ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_CALL, KETL_ASM_X86_PTRSIZE, 
                //     modrm_stack(p_builder, KETL_ASM_X86_PTRSIZE, callee_arg.stack_offset));
                // ketl_asm_x86_arg_info_t output_arg = hir_get_arg_stack_offset(p_builder, p_hir_info->output_var);
                // ketl_asm_x86_insert_rm_reg(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, 
                //     modrm_stack(p_builder, KETL_ASM_X86_PTRSIZE, output_arg.stack_offset), KETL_ASM_X86_AX);
                continue;
            }
            case KETL_HIR_CREATE_ARRAY: {
                ketl_hir_create_array_t* p_hir_info = (ketl_hir_create_array_t*)p_instr;

                //ketl_gc_create_array(&p_builder->p_state->gc, p_hir->p_used_types[p_hir_info->type], size, 0);
                
                // lets do this first, we might use other registers while getting count
                // TODO use correct size
                push_mov_from_stack(get_reg_parameter(p_builder->abi_type, 2), p_hir_info->count_var_id, KETL_ASM_X86_64B, p_builder);
                ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, MODRM_REG(get_reg_parameter(p_builder->abi_type, 3)), 0); // flags
                ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, MODRM_REG(get_reg_parameter(p_builder->abi_type, 1)), (uint64_t)p_hir->p_used_types[p_hir_info->type]);
                ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, MODRM_REG(get_reg_parameter(p_builder->abi_type, 0)), (uint64_t)&p_builder->p_state->gc);

                void* func_address;
                #define KETL_POINTER_CONVERTER
                #define KETL_POINTER_CONVERTER_ARG  &ketl_gc_create_array
                #define KETL_POINTER_CONVERTER_VAR  func_address
                #define KETL_POINTER_CONVERTER_TYPE void*
                #include "meta.i"
                 
                ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, MODRM_REG(KETL_ASM_X86_AX), (uint64_t)func_address);

                // call create from gc
                ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_CALL, KETL_ASM_X86_PTRSIZE, MODRM_REG(KETL_ASM_X86_AX));

                // TODO call constructor

                push_mov_to_stack(p_hir_info->output_var, KETL_ASM_X86_AX, KETL_ASM_X86_PTRSIZE, p_builder);

                // ANN_ASSERT(false);
                // ketl_asm_x86_arg_info_t callee_arg = hir_get_arg_stack_offset(p_builder, p_hir_info->type); // TODO wrong!

                // // TODO FIX allow other types to be called
                // ANN_ASSERT(callee_arg.p_type->type == KETL_TYPE_CFUNCTION);

                // for (uint32_t i = p_hir_info->arguments_count - 1; i != (uint32_t) -1; --i) {
                //     ketl_asm_x86_arg_info_t arg = hir_get_arg_stack_offset(p_builder, p_hir_info->arguments[i]);

                //     if (i >= ANN_ARRAY_SIZE(a_parameter_regs)) {
                //         ANN_ASSERT(false);
                //     }
                //     // TODO FIX
                //     // get type size and use appropriate
                //     ketl_asm_x86_size_t size = KETL_ASM_X86_64B;
                //     ketl_asm_x86_insert_reg_rm(p_builder, KETL_ASM_X86_MOV, size, 
                //         a_parameter_regs[i], modrm_stack(p_builder, size, arg.stack_offset));
                // }

                // ketl_asm_x86_insert_rm(p_builder, KETL_ASM_X86_CALL, KETL_ASM_X86_PTRSIZE, 
                //     modrm_stack(p_builder, KETL_ASM_X86_PTRSIZE, callee_arg.stack_offset));
                // ketl_asm_x86_arg_info_t output_arg = hir_get_arg_stack_offset(p_builder, p_hir_info->output_var);
                // ketl_asm_x86_insert_rm_reg(p_builder, KETL_ASM_X86_MOV, KETL_ASM_X86_PTRSIZE, 
                //     modrm_stack(p_builder, KETL_ASM_X86_PTRSIZE, output_arg.stack_offset), KETL_ASM_X86_AX);
                continue;
            }
            case KETL_HIR_JUMP: {
                ketl_hir_jump_t* p_hir_info = (ketl_hir_jump_t*)p_instr;

                if (!first_instr_in_block) {
                    ketl_hir_block_index_t dest = get_first_non_empty_block(p_hir, p_hir_info->block_index);
                    
                    if (p_hir->p_block_offsets[dest] != i + ketl_hir_decode_size(p_hir, i)) {
                        ketl_asm_x86_insert_jimm(p_builder, KETL_ASM_X86_JMP, KETL_ASM_X86_NONESIZE, dest);
                    }
                }
                
                first_instr_in_block = true;
                continue;
            }
            case KETL_HIR_JUMP_IF_TRUE: {
                ketl_hir_jump_if_t* p_hir_info = (ketl_hir_jump_if_t*)p_instr;

                ketl_hir_block_index_t true_dest = get_first_non_empty_block(p_hir, p_hir_info->true_block);
                ketl_hir_block_index_t false_dest = get_first_non_empty_block(p_hir, p_hir_info->false_block);
                
                push_mov_from_stack(KETL_ASM_X86_AX, p_hir_info->expr_var, KETL_ASM_X86_8B, p_builder);
                ketl_asm_x86_insert_rm_reg(p_builder, KETL_ASM_X86_TEST, KETL_ASM_X86_8B, 
                    MODRM_REG(KETL_ASM_X86_AX), KETL_ASM_X86_AX);
                ketl_asm_x86_insert_jimm(p_builder, KETL_ASM_X86_JNE, KETL_ASM_X86_NONESIZE, true_dest);

                if (p_hir->p_block_offsets[false_dest] != i + ketl_hir_decode_size(p_hir, i)) {
                    ketl_asm_x86_insert_jimm(p_builder, KETL_ASM_X86_JMP, KETL_ASM_X86_NONESIZE, false_dest);
                }
                
                first_instr_in_block = true;
                continue;
            }
            case KETL_HIR_RETURN: {
                ketl_asm_x86_insert_rm_imm(p_builder, KETL_ASM_X86_ADD, KETL_ASM_X86_PTRSIZE, MODRM_REG(KETL_ASM_X86_SP), stack_reserved_size);
                ketl_asm_x86_insert(p_builder, KETL_ASM_X86_RET);

                first_instr_in_block = true;
                continue;
            }
        }
    }

    for (uint32_t i = 0u; i < p_builder->instrs.size; ++i) {
        switch (p_builder->instrs.p_data[i].tag) {
            case KETL_ASM_X86_JMP:
            case KETL_ASM_X86_JNE: {
                ketl_hir_block_index_t block = (ketl_hir_block_index_t)p_builder->instrs.p_data[i].imm64;
                ketl_hir_to_asm_offsets_t_bucket* p_bucket = ketl_hir_to_asm_offsets_t_get_or_null(&p_builder->hir_to_asm_offsets, p_hir->p_block_offsets[block]); 
                p_builder->instrs.p_data[i].imm64 = p_bucket->value;
                continue;
            }
        }
    }

    ketl_asm_x86_bake_offsets(p_builder->instrs.p_data, p_builder->instrs.size, p_builder->p_state->p_allocator);

    *p_asm_x86 = (ketl_asm_x86_t){
        .p_allocator = p_builder->p_state->p_allocator,
        .p_instrs = ketl_alloc(p_builder->p_state->p_allocator, sizeof(ketl_asm_x86_instr_t) * p_builder->instrs.size), 
        .size = p_builder->instrs.size,
    };

    ketl_memcpy(p_asm_x86->p_instrs, p_builder->instrs.p_data, sizeof(ketl_asm_x86_instr_t) * p_builder->instrs.size);
}

uint8_t* ketl_asm_x86_compile(ketl_asm_x86_t* p_asm_x86, uint32_t* p_opcodes_size, const ketl_allocator* p_allocator) {
    opcodes_t opcodes;
    opcodes_t_init(&opcodes, p_asm_x86->size * 2, p_allocator);

    for (uint32_t i = 0u; i < p_asm_x86->size; ++i) {
        ketl_asm_x86_push_opcode(&opcodes, p_asm_x86->p_instrs, i);
    }

    *p_opcodes_size = opcodes.size;
    return opcodes.p_data;
}

void ketl_asm_x86_deinit(ketl_asm_x86_t* p_asm_x86) {
    ketl_free(p_asm_x86->p_allocator, p_asm_x86->p_instrs);
}

///////////////////////////////////////////////////////////////////////////////////////////////

static uint32_t ketl_asm_x86_format_reg(ketl_asm_x86_reg_t reg, ketl_asm_x86_size_t size, char* p_buffer, uint32_t buffer_size) {
    ANN_SWITCH_STRICT (reg) {
        case KETL_ASM_X86_AX:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "al");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "ax");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "eax");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "rax");
            }
        case KETL_ASM_X86_CX:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "cl");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "cx");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "ecx");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "rcx");
            }
        case KETL_ASM_X86_DX:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "dl");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "dx");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "edx");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "rdx");
            }
        case KETL_ASM_X86_BX:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "bl");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "bx");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "ebx");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "rbx");
            }
        case KETL_ASM_X86_SP:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "spl");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "sp");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "esp");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "rsp");
            }
        case KETL_ASM_X86_BP:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "bpl");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "bp");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "ebp");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "rbp");
            }
        case KETL_ASM_X86_SI:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "sil");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "si");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "esi");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "rsi");
            }
        case KETL_ASM_X86_DI:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "dil");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "di");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "edi");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "rdi");
            }
        case KETL_ASM_X86_R8:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "r8l");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "r8w");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "r8d");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "r8");
            }
        case KETL_ASM_X86_R9:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "r9l");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "r9w");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "r9d");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "r9");
            }
        case KETL_ASM_X86_R10:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "r10l");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "r10w");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "r10d");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "r10");
            }
        case KETL_ASM_X86_R11:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "r11l");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "r11w");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "r11d");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "r11");
            }
        case KETL_ASM_X86_R12:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "r12l");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "r12w");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "r12d");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "r12");
            }
        case KETL_ASM_X86_R13:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "r13l");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "r13w");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "r13d");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "r13");
            }
        case KETL_ASM_X86_R14:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "r14l");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "r14w");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "r14d");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "r14");
            }
        case KETL_ASM_X86_R15:
            ANN_SWITCH_STRICT (size) {
                case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size,  "r15l");
                case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "r15w");
                case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "r15d");
                case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "r15");
            }
        case KETL_ASM_X86_REG_NONE:
            return snprintf(p_buffer, buffer_size, "$reg-error");
    }
}

static uint32_t ketl_asm_x86_format_size(ketl_asm_x86_size_t size, char* p_buffer, uint32_t buffer_size) {
    ANN_SWITCH_STRICT (size) {
        case KETL_ASM_X86_8B: return snprintf(p_buffer, buffer_size, "byte");
        case KETL_ASM_X86_16B: return snprintf(p_buffer, buffer_size, "word");
        case KETL_ASM_X86_32B: return snprintf(p_buffer, buffer_size, "dword");
        case KETL_ASM_X86_64B: return snprintf(p_buffer, buffer_size, "qword");
    }
}

static uint32_t ketl_asm_x86_format_modrm(ketl_state* p_state, ketl_asm_x86_modrm_t modrm, ketl_asm_x86_size_t size, char* p_buffer, uint32_t buffer_size) {
    if (!modrm.indir) {
        if (modrm.label) {
            return snprintf(p_buffer, buffer_size, "%s", ketl_atomic_strings_get_pointer(&p_state->atomic_strings, modrm.s_literal));
        }
        return ketl_asm_x86_format_reg(modrm.base, size, p_buffer, buffer_size);
    }

    uint32_t printed = 0;
    printed += ketl_asm_x86_format_size(size, p_buffer + printed, buffer_size - printed);
    printed += snprintf(p_buffer + printed, buffer_size - printed, " ptr [");

    if (modrm.label) {
        const char* p_label_name = ketl_atomic_strings_get_pointer(&p_state->atomic_strings, modrm.s_literal);
        printed += snprintf(p_buffer + printed, buffer_size - printed, "rip + %s]", p_label_name);
        return printed;
    }

    printed += ketl_asm_x86_format_reg(modrm.base, KETL_ASM_X86_PTRSIZE, p_buffer + printed, buffer_size - printed);
    
    if (modrm.index != KETL_ASM_X86_REG_NONE) {
        printed += snprintf(p_buffer + printed, buffer_size - printed, " + (");
        printed += ketl_asm_x86_format_reg(modrm.index, KETL_ASM_X86_PTRSIZE, p_buffer + printed, buffer_size - printed);
        printed += snprintf(p_buffer + printed, buffer_size - printed, " * %"PRIu8")", modrm.scale);
    }

    if (modrm.disp != 0) {
        if (modrm.label) {
            printed += snprintf(p_buffer + printed, buffer_size - printed, " + %s", ketl_atomic_strings_get_pointer(&p_state->atomic_strings, modrm.s_literal));
        } else {
            if (modrm.disp >= 0) {
                printed += snprintf(p_buffer + printed, buffer_size - printed, " + 0x%"PRIx32, modrm.disp);
            } else {
                printed += snprintf(p_buffer + printed, buffer_size - printed, " - 0x%"PRIx64, -((uint64_t)modrm.disp));
            }
        }
    }

    printed += snprintf(p_buffer + printed, buffer_size - printed, "]");

    return printed;
}

static uint32_t ketl_asm_x86_format_instr(ketl_state* p_state, ketl_asm_x86_instr_t* p_instrs, ketl_asm_x86_offset_t index, char* p_buffer, uint32_t buffer_size) {
    ketl_asm_x86_instr_t* p_instr = &p_instrs[index];
    uint32_t printed = 0;

    if (p_instr->tag == KETL_ASM_LABEL) {
        printed += snprintf(p_buffer + printed, buffer_size - printed, "%s:",
            ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_instr->modrm.s_literal));
        return printed;
    } else {
        printed += snprintf(p_buffer + printed, buffer_size - printed, "    ");
    }

    ANN_SWITCH_STRICT (p_instr->tag) {
        case KETL_ASM_X86_MOV:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "mov ");
            break;
        case KETL_ASM_X86_ADD:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "add ");
            break;
        case KETL_ASM_X86_SUB:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "sub ");
            break;
        case KETL_ASM_X86_IMUL:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "imul ");
            break;

        case KETL_ASM_X86_CMP:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "cmp ");
            break;
        case KETL_ASM_X86_TEST:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "test ");
            break;
            
        case KETL_ASM_X86_SETE:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "sete ");
            break;
        case KETL_ASM_X86_SETNE:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "setne ");
            break;
        case KETL_ASM_X86_SETG:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "setg ");
            break;
        case KETL_ASM_X86_SETGE:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "setge ");
            break;
        case KETL_ASM_X86_SETL:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "setl ");
            break;
        case KETL_ASM_X86_SETLE:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "setle ");
            break;

        case KETL_ASM_X86_JMP:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "jmp ");
            break;
        case KETL_ASM_X86_JNE:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "jne ");
            break;

        case KETL_ASM_X86_CALL:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "call ");
            break;
        case KETL_ASM_X86_RET:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "ret");
            break;
    }

    ANN_SWITCH_STRICT (p_instr->arg_type) {
        case KETL_ASM_X86_EMPTY:
            break;
        case KETL_ASM_X86_M:
            printed += ketl_asm_x86_format_modrm(p_state, p_instr->modrm, p_instr->size, p_buffer + printed, buffer_size - printed);
            break;
        case KETL_ASM_X86_RM:
            printed += ketl_asm_x86_format_reg(p_instr->reg, p_instr->size, p_buffer + printed, buffer_size - printed);
            printed += snprintf(p_buffer + printed, buffer_size - printed, ", ");
            printed += ketl_asm_x86_format_modrm(p_state, p_instr->modrm, p_instr->size, p_buffer + printed, buffer_size - printed);
            break;
        case KETL_ASM_X86_MR:
            printed += ketl_asm_x86_format_modrm(p_state, p_instr->modrm, p_instr->size, p_buffer + printed, buffer_size - printed);
            printed += snprintf(p_buffer + printed, buffer_size - printed, ", ");
            printed += ketl_asm_x86_format_reg(p_instr->reg, p_instr->size, p_buffer + printed, buffer_size - printed);
            break;
        case KETL_ASM_X86_MI:
            printed += ketl_asm_x86_format_modrm(p_state, p_instr->modrm, p_instr->size, p_buffer + printed, buffer_size - printed);
            ANN_SWITCH_STRICT (p_instr->size) {
                case KETL_ASM_X86_8B:
                    printed += snprintf(p_buffer + printed, buffer_size - printed, ", 0x%"PRIx8, p_instr->imm8);
                    break;
                case KETL_ASM_X86_16B:
                    printed += snprintf(p_buffer + printed, buffer_size - printed, ", 0x%"PRIx16, p_instr->imm16);
                    break;
                case KETL_ASM_X86_32B:
                    printed += snprintf(p_buffer + printed, buffer_size - printed, ", 0x%"PRIx32, p_instr->imm32);
                    break;
                case KETL_ASM_X86_64B:
                    printed += snprintf(p_buffer + printed, buffer_size - printed, ", 0x%"PRIx64, p_instr->imm64);
                    break;
            }
            break;
        case KETL_ASM_X86_JI:
            printed += snprintf(p_buffer + printed, buffer_size - printed, "%s", 
                ketl_atomic_strings_get_pointer(&p_state->atomic_strings, p_instrs[p_instr->goto_offset].modrm.s_literal));
            break;
    }

    return printed;
}

uint32_t ketl_asm_x86_format(ketl_state* p_state, ketl_asm_x86_t* p_asm_x86, char* p_buffer, uint32_t buffer_size, bool verbose) {
    opcodes_t opcodes;
    opcodes_t_init(&opcodes, 16, p_state->p_allocator);

    uint32_t printed = 0;
    
    for (uint32_t i = 0u; i < p_asm_x86->size; ++i) {
        if (verbose) {
            printed += snprintf(p_buffer + printed, buffer_size - printed, "# 0x%08"PRIx64": ", p_asm_x86->p_instrs[i].opcode_offset);

            ketl_asm_x86_push_opcode(&opcodes, p_asm_x86->p_instrs, i);
            printed += ketl_asm_format_opcodes(opcodes.p_data, opcodes.size, p_buffer + printed, buffer_size - printed);
            opcodes.size = 0;

            printed += snprintf(p_buffer + printed, buffer_size - printed, "\n");
        }
        printed += ketl_asm_x86_format_instr(p_state, p_asm_x86->p_instrs, i, p_buffer + printed, buffer_size - printed);
        printed += snprintf(p_buffer + printed, buffer_size - printed, "\n");
    }

    opcodes_t_deinit(&opcodes);

    return printed;
}
