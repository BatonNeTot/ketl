//🫖ketl
#include "x86.h"

KETL_VECTOR_DEFINITION(opcodes, uint8_t)

static void push_mov(opcodes* pOpcodes, x86_op_struct* pOpStruct) {
    if (pOpStruct->size == KETL_SIZE_16B) {
        opcodes_push_back_copy(pOpcodes, 0x66); // set 16-bit operand size
    }

    REXByte rex = {
        .prefix = REX_PREFIX,
        .w = 0, .r = 0, .x = 0, .b = 0};
    uint32_t rexOffset = 0;

    if (pOpStruct->size == KETL_SIZE_64B) {
        // set 64-bit operand size
        rex.w = 1;
        rexOffset = pOpcodes->size;
        opcodes_push_back_ref(pOpcodes, (uint8_t*)(&rex));
    } else if (pOpStruct->size == KETL_SIZE_8B &&
        (
            (
                (pOpStruct->firstArgType == KETL_ARG_REG ||
                pOpStruct->firstArgType == KETL_ARG_REG_MEM) &&
                (pOpStruct->firstArg >= KETL_REG_SP || 
                pOpStruct->firstArg <= KETL_REG_DI)
            ) ||
            (
                (pOpStruct->secondArgType == KETL_ARG_REG ||
                pOpStruct->secondArgType == KETL_ARG_REG_MEM) &&
                (pOpStruct->secondArg >= KETL_REG_SP || 
                pOpStruct->secondArg <= KETL_REG_DI)
            )
        )) {
        // for accessing spl, bpl, sil and dil rex be inserted
        rexOffset = pOpcodes->size;
        opcodes_push_back_ref(pOpcodes, (uint8_t*)(&rex));
    } else if (
        (
            (pOpStruct->firstArgType == KETL_ARG_REG ||
            pOpStruct->firstArgType == KETL_ARG_REG_MEM) &&
            pOpStruct->firstArg >= KETL_REG_R8) ||
        (
            (pOpStruct->secondArgType == KETL_ARG_REG ||
            pOpStruct->secondArgType == KETL_ARG_REG_MEM) &&
            pOpStruct->secondArg >= KETL_REG_R8
        )) {
        // for accessing r8-r15 registers
        // additional flags must be set, but they will be desided later
        rexOffset = pOpcodes->size;
        opcodes_push_back_ref(pOpcodes, (uint8_t*)(&rex));
    }
    
    KETL_SWITCH_STRICT (pOpStruct->firstArgType) {
    case KETL_ARG_REG:
        KETL_SWITCH_STRICT (pOpStruct->secondArgType) {
        case KETL_ARG_REG:
        case KETL_ARG_REG_MEM:
        case KETL_ARG_RSP_MEM_DISP:
        case KETL_ARG_RBP_MEM_DISP:
            if (pOpStruct->size == KETL_SIZE_8B) {
                opcodes_push_back_copy(pOpcodes, 0x8a);
            } else {
                opcodes_push_back_copy(pOpcodes, 0x8b);
            }
            if (pOpStruct->firstArg >= KETL_REG_R8) {
                pOpStruct->firstArg -= KETL_REG_R8;
                ((REXByte*)(pOpcodes->pData + rexOffset))->r = 1;
            }
            if ((pOpStruct->secondArgType == KETL_ARG_REG ||
                pOpStruct->secondArgType == KETL_ARG_REG_MEM) &&
                pOpStruct->secondArg >= KETL_REG_R8) {
                pOpStruct->secondArg -= KETL_REG_R8;
                ((REXByte*)(pOpcodes->pData + rexOffset))->b = 1;
            }
            
            if (pOpStruct->secondArgType == KETL_ARG_REG) {
                // reg
                MODRMByte modrm = {
                    .mod = MODRM_MOD_DIR,
                    .reg = pOpStruct->firstArg,
                    .rm = pOpStruct->secondArg
                };
                opcodes_push_back_ref(pOpcodes, (uint8_t*)&modrm);
            } else {
                if (pOpStruct->secondArgType == KETL_ARG_REG_MEM) {
                    // [reg]
                    MODRMByte modrm = {
                        .mod = MODRM_MOD_IND,
                        .reg = pOpStruct->firstArg,
                        .rm = pOpStruct->secondArg
                    };
                    opcodes_push_back_ref(pOpcodes, (uint8_t*)&modrm);
                } else {
                    // [SIB]
                    MODRMByte modrm = {
                        .mod = MODRM_MOD_IND_DIS32,
                        .reg = pOpStruct->firstArg,
                        .rm = KETL_REG_SP
                    };
                    opcodes_push_back_ref(pOpcodes, (uint8_t*)&modrm);
                    // [RSP + disp32] or [RBP + disp32]
                    SIBByte sib = {
                        .scale =  SIB_SCALE_1,
                        .index = KETL_REG_SP,
                        .base = pOpStruct->secondArgType == KETL_ARG_RSP_MEM_DISP
                            ? KETL_REG_SP : KETL_REG_BP
                    };
                    opcodes_push_back_ref(pOpcodes, (uint8_t*)&sib);
                    uint32_t secondArg = pOpStruct->secondArg;
                    opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
                }
            }
            return;
        case KETL_ARG_IMM:
            if (pOpStruct->size == KETL_SIZE_8B) {
                opcodes_push_back_copy(pOpcodes, 0xb0 + pOpStruct->firstArg);
            } else {
                opcodes_push_back_copy(pOpcodes, 0xb8 + pOpStruct->firstArg);
            }
            if (pOpStruct->firstArg >= KETL_REG_R8) {
                pOpStruct->firstArg -= KETL_REG_R8;
                ((REXByte*)(pOpcodes->pData + rexOffset))->r = 1;
            }
            KETL_SWITCH_STRICT (pOpStruct->size) {
            case KETL_SIZE_8B: {
                uint8_t secondArg = pOpStruct->secondArg;
                opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
                break;
            }
            case KETL_SIZE_16B: {
                uint16_t secondArg = pOpStruct->secondArg;
                opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
                break;
            }
            case KETL_SIZE_32B: {
                uint32_t secondArg = pOpStruct->secondArg;
                opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
                break;
            }
            case KETL_SIZE_64B: {
                uint64_t secondArg = pOpStruct->secondArg;
                opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
                break;
            }
            }
            return;
        case KETL_ARG_IMM_MEM:
            if (pOpStruct->size == KETL_SIZE_8B) {
                opcodes_push_back_copy(pOpcodes, 0xa0);
            } else {
                opcodes_push_back_copy(pOpcodes, 0xa1);
            }
            uint64_t secondArg = pOpStruct->secondArg;
            opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
            if (pOpStruct->firstArg != KETL_REG_AX) {
                pOpStruct->secondArgType = KETL_ARG_REG;
                pOpStruct->secondArg = KETL_REG_AX;
                push_mov(pOpcodes, pOpStruct);
            }
            return;
        return;
        }
    case KETL_ARG_REG_MEM:
    case KETL_ARG_RSP_MEM_DISP:
    case KETL_ARG_RBP_MEM_DISP:
        KETL_SWITCH_STRICT (pOpStruct->secondArgType) {
        case KETL_ARG_REG:
            if (pOpStruct->size == KETL_SIZE_8B) {
                opcodes_push_back_copy(pOpcodes, 0x88);
            } else {
                opcodes_push_back_copy(pOpcodes, 0x89);
            }
            if (pOpStruct->firstArgType == KETL_ARG_REG_MEM &&
                pOpStruct->firstArg >= KETL_REG_R8) {
                pOpStruct->firstArg -= KETL_REG_R8;
                ((REXByte*)(pOpcodes->pData + rexOffset))->b = 1;
            }
            if (pOpStruct->secondArg >= KETL_REG_R8) {
                pOpStruct->secondArg -= KETL_REG_R8;
                ((REXByte*)(pOpcodes->pData + rexOffset))->r = 1;
            }


            if (pOpStruct->firstArgType == KETL_ARG_REG_MEM) {
                // [reg]
                MODRMByte modrm = {
                    .mod = MODRM_MOD_IND,
                    .reg = pOpStruct->secondArg,
                    .rm = pOpStruct->firstArg
                };
                opcodes_push_back_ref(pOpcodes, (uint8_t*)&modrm);
            } else {
                // [SIB]
                MODRMByte modrm = {
                    .mod = MODRM_MOD_IND_DIS32,
                    .reg = pOpStruct->secondArg,
                    .rm = KETL_REG_SP
                };
                opcodes_push_back_ref(pOpcodes, (uint8_t*)&modrm);
                // [RSP + disp32] or [RBP + disp32]
                SIBByte sib = {
                    .scale =  SIB_SCALE_1,
                    .index = KETL_REG_SP,
                    .base = pOpStruct->firstArgType == KETL_ARG_RSP_MEM_DISP
                        ? KETL_REG_SP : KETL_REG_BP
                };
                opcodes_push_back_ref(pOpcodes, (uint8_t*)&sib);
                uint32_t firstArg = pOpStruct->firstArg;
                opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&firstArg, sizeof(firstArg));
            }
            return;
        return;
        }
    case KETL_ARG_IMM_MEM:
        KETL_SWITCH_STRICT (pOpStruct->secondArgType) {
        case KETL_ARG_REG:
            if (pOpStruct->size == KETL_SIZE_8B) {
                opcodes_push_back_copy(pOpcodes, 0xa2);
            } else {
                opcodes_push_back_copy(pOpcodes, 0xa3);
            }
            uint64_t firstArg = pOpStruct->firstArg;
            opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&firstArg, sizeof(firstArg));
            if (pOpStruct->secondArg != KETL_REG_AX) {
                pOpStruct->firstArgType = KETL_ARG_REG;
                pOpStruct->firstArg = KETL_REG_AX;
                push_mov(pOpcodes, pOpStruct);
            }
            return;
        return;
        }
    return;
    }
}

void push_opcode(opcodes* pOpcodes, x86_op_struct* pOpStruct) {
    switch (pOpStruct->opCode) {
    case KETL_OP_MOV:
        push_mov(pOpcodes, pOpStruct);
    }
}
