//🫖ketl
#include "x86.h"

KETL_NAMED_VECTOR_DEFINITION(opcodes, uint8_t)

static void push_mov(ketl_vector_opcodes* pOpcodes, x86_op_struct* pOpStruct) {
    if (pOpStruct->size == KETL_SIZE_16B) {
        ketl_vector_opcodes_push_back_copy(pOpcodes, 0x66); // set 16-bit operand size
    }

    REXByte rex = {
        .prefix = REX_PREFIX,
        .w = 0, .r = 0, .x = 0, .b = 0};
    uint32_t rexOffset = 0;

    if (pOpStruct->size == KETL_SIZE_64B) {
        // set 64-bit operand size
        rex.w = 1;
        rexOffset = pOpcodes->size;
        ketl_vector_opcodes_push_back_ref(pOpcodes, (uint8_t*)(&rex));
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
        ketl_vector_opcodes_push_back_ref(pOpcodes, (uint8_t*)(&rex));
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
        ketl_vector_opcodes_push_back_ref(pOpcodes, (uint8_t*)(&rex));
    }
    
    switch (pOpStruct->firstArgType) {
    case KETL_ARG_REG:
        switch (pOpStruct->secondArgType) {
        case KETL_ARG_REG:
        case KETL_ARG_REG_MEM:
        case KETL_ARG_RSP_MEM_DISP:
        case KETL_ARG_RBP_MEM_DISP:
            if (pOpStruct->size == KETL_SIZE_8B) {
                ketl_vector_opcodes_push_back_copy(pOpcodes, 0x8a);
            } else {
                ketl_vector_opcodes_push_back_copy(pOpcodes, 0x8b);
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
                ketl_vector_opcodes_push_back_ref(pOpcodes, (uint8_t*)&modrm);
            } else {
                if (pOpStruct->secondArgType == KETL_ARG_REG_MEM) {
                    // [reg]
                    MODRMByte modrm = {
                        .mod = MODRM_MOD_IND,
                        .reg = pOpStruct->firstArg,
                        .rm = pOpStruct->secondArg
                    };
                    ketl_vector_opcodes_push_back_ref(pOpcodes, (uint8_t*)&modrm);
                } else {
                    // [SIB]
                    MODRMByte modrm = {
                        .mod = MODRM_MOD_IND_DIS32,
                        .reg = pOpStruct->firstArg,
                        .rm = KETL_REG_SP
                    };
                    ketl_vector_opcodes_push_back_ref(pOpcodes, (uint8_t*)&modrm);
                    // [RSP + disp32] or [RBP + disp32]
                    SIBByte sib = {
                        .scale =  SIB_SCALE_1,
                        .index = KETL_REG_SP,
                        .base = pOpStruct->secondArgType == KETL_ARG_RSP_MEM_DISP
                            ? KETL_REG_SP : KETL_REG_BP
                    };
                    ketl_vector_opcodes_push_back_ref(pOpcodes, (uint8_t*)&sib);
                    uint32_t secondArg = pOpStruct->secondArg;
                    ketl_vector_opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
                }
            }
            return;
        case KETL_ARG_IMM:
            if (pOpStruct->size == KETL_SIZE_8B) {
                ketl_vector_opcodes_push_back_copy(pOpcodes, 0xb0 + pOpStruct->firstArg);
            } else {
                ketl_vector_opcodes_push_back_copy(pOpcodes, 0xb8 + pOpStruct->firstArg);
            }
            if (pOpStruct->firstArg >= KETL_REG_R8) {
                pOpStruct->firstArg -= KETL_REG_R8;
                ((REXByte*)(pOpcodes->pData + rexOffset))->r = 1;
            }
            switch (pOpStruct->size) {
            case KETL_SIZE_8B: {
                uint8_t secondArg = pOpStruct->secondArg;
                ketl_vector_opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
            }
            case KETL_SIZE_16B: {
                uint16_t secondArg = pOpStruct->secondArg;
                ketl_vector_opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
            }
            case KETL_SIZE_32B: {
                uint32_t secondArg = pOpStruct->secondArg;
                ketl_vector_opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
            }
            case KETL_SIZE_64B: {
                uint64_t secondArg = pOpStruct->secondArg;
                ketl_vector_opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
            }
            return;
            KETL_NODEFAULT()
            }
        case KETL_ARG_IMM_MEM:
            if (pOpStruct->size == KETL_SIZE_8B) {
                ketl_vector_opcodes_push_back_copy(pOpcodes, 0xa0);
            } else {
                ketl_vector_opcodes_push_back_copy(pOpcodes, 0xa1);
            }
            uint64_t secondArg = pOpStruct->secondArg;
            ketl_vector_opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&secondArg, sizeof(secondArg));
            if (pOpStruct->firstArg != KETL_REG_AX) {
                pOpStruct->secondArgType = KETL_ARG_REG;
                pOpStruct->secondArg = KETL_REG_AX;
                push_mov(pOpcodes, pOpStruct);
            }
            return;
        KETL_NODEFAULT()
        }
    case KETL_ARG_REG_MEM:
    case KETL_ARG_RSP_MEM_DISP:
    case KETL_ARG_RBP_MEM_DISP:
        switch (pOpStruct->secondArgType) {
        case KETL_ARG_REG:
            if (pOpStruct->size == KETL_SIZE_8B) {
                ketl_vector_opcodes_push_back_copy(pOpcodes, 0x88);
            } else {
                ketl_vector_opcodes_push_back_copy(pOpcodes, 0x89);
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
                ketl_vector_opcodes_push_back_ref(pOpcodes, (uint8_t*)&modrm);
            } else {
                // [SIB]
                MODRMByte modrm = {
                    .mod = MODRM_MOD_IND_DIS32,
                    .reg = pOpStruct->secondArg,
                    .rm = KETL_REG_SP
                };
                ketl_vector_opcodes_push_back_ref(pOpcodes, (uint8_t*)&modrm);
                // [RSP + disp32] or [RBP + disp32]
                SIBByte sib = {
                    .scale =  SIB_SCALE_1,
                    .index = KETL_REG_SP,
                    .base = pOpStruct->firstArgType == KETL_ARG_RSP_MEM_DISP
                        ? KETL_REG_SP : KETL_REG_BP
                };
                ketl_vector_opcodes_push_back_ref(pOpcodes, (uint8_t*)&sib);
                uint32_t firstArg = pOpStruct->firstArg;
                ketl_vector_opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&firstArg, sizeof(firstArg));
            }
            return;
        KETL_NODEFAULT()
        }
    case KETL_ARG_IMM_MEM:
        switch (pOpStruct->secondArgType) {
        case KETL_ARG_REG:
            if (pOpStruct->size == KETL_SIZE_8B) {
                ketl_vector_opcodes_push_back_copy(pOpcodes, 0xa2);
            } else {
                ketl_vector_opcodes_push_back_copy(pOpcodes, 0xa3);
            }
            uint64_t firstArg = pOpStruct->firstArg;
            ketl_vector_opcodes_push_back_ref_n(pOpcodes, (uint8_t*)&firstArg, sizeof(firstArg));
            if (pOpStruct->secondArg != KETL_REG_AX) {
                pOpStruct->firstArgType = KETL_ARG_REG;
                pOpStruct->firstArg = KETL_REG_AX;
                push_mov(pOpcodes, pOpStruct);
            }
            return;
        KETL_NODEFAULT()
        }
    KETL_NODEFAULT()
    }
}

void push_opcode(ketl_vector_opcodes* pOpcodes, x86_op_struct* pOpStruct) {
    switch (pOpStruct->opCode) {
    case KETL_OP_MOV:
        push_mov(pOpcodes, pOpStruct);
    }
}

/*
uint64_t push_load_argument_into_reg(ketl_vector_opcodes* pOpcodes, ketl_ir_argument* argument, uint8_t sizeMacro, uint8_t regMacro) {
    uint64_t size = 0;
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: 
#if KETL_OS_WINDOWS
        EXEC_OPCODE_REG_RSP_DISP(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, (int32_t)argument->stack);
#else
        EXEC_OPCODE_REG_RBP_DISP(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, (int32_t)argument->stack);
#endif
        return;
    case KETL_IR_ARGUMENT_TYPE_POINTER: 
        EXEC_OPCODE_REG_IMM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, (uint64_t)argument->pointer);
        return;
    case KETL_IR_ARGUMENT_TYPE_REFERENCE: 
        EXEC_OPCODE_REG_IMM_MEM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, (uint64_t)argument->pointer);
        return;
    case KETL_IR_ARGUMENT_TYPE_INT8:
        EXEC_OPCODE_REG_IMM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int8);
        return;
    case KETL_IR_ARGUMENT_TYPE_INT16:
        EXEC_OPCODE_REG_IMM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int16);
        return;
    case KETL_IR_ARGUMENT_TYPE_INT32:
        EXEC_OPCODE_REG_IMM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int32);
        return;
    case KETL_IR_ARGUMENT_TYPE_INT64:
        EXEC_OPCODE_REG_IMM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int32);
        return;
    KETL_NODEFAULT()
    }
    return 0;
}

uint64_t push_load_reg_into_argument(ketl_vector_opcodes* pOpcodes, ketl_ir_argument* argument, uint8_t sizeMacro, uint8_t regMacro) {
    uint8_t size = 0;
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: 
#if KETL_OS_WINDOWS
        EXEC_OPCODE_RSP_DISP_REG(pOpcodes, size, sizeMacro, KETL_OP_MOV, (int32_t)argument->stack, regMacro);
#else
        EXEC_OPCODE_RBP_DISP_REG(pOpcodes, size, sizeMacro, KETL_OP_MOV, (int32_t)argument->stack, regMacro);
#endif
        return;
    case KETL_IR_ARGUMENT_TYPE_REFERENCE: 
        EXEC_OPCODE_IMM_MEM_REG(pOpcodes, size, sizeMacro, KETL_OP_MOV, (uint64_t)argument->pointer, regMacro);
        return;
    KETL_NODEFAULT()
    }
    return 0;
}

uint64_t push_load_const8_into_reg(ketl_vector_opcodes* pOpcodes, ketl_ir_argument* argument, uint8_t sizeMacro, uint8_t regMacro) {
    uint64_t size = 0;
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: 
#if KETL_OS_WINDOWS
        EXEC_OPCODE_REG_RSP_DISP(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, (int32_t)argument->stack);
#else
        EXEC_OPCODE_REG_RBP_DISP(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, (int32_t)argument->stack);
#endif
        return;
    case KETL_IR_ARGUMENT_TYPE_POINTER: 
        EXEC_OPCODE_REG_IMM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, (uint64_t)argument->pointer);
        return;
    case KETL_IR_ARGUMENT_TYPE_REFERENCE: 
        EXEC_OPCODE_REG_IMM_MEM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, (uint64_t)argument->pointer);
        return;
    case KETL_IR_ARGUMENT_TYPE_INT8:
        EXEC_OPCODE_REG_IMM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int8);
        return;
    case KETL_IR_ARGUMENT_TYPE_INT16:
        EXEC_OPCODE_REG_IMM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int16);
        return;
    case KETL_IR_ARGUMENT_TYPE_INT32:
        EXEC_OPCODE_REG_IMM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int32);
        return;
    case KETL_IR_ARGUMENT_TYPE_INT64:
        EXEC_OPCODE_REG_IMM(pOpcodes, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int32);
        return;
    KETL_NODEFAULT()
    }
    return 0;
}
*/