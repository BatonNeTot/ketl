//🫖ketl
#ifndef ketl_compiler_assembler_x86_h
#define ketl_compiler_assembler_x86_h

#include "containers/vector.h"

#include "ketl/utils.h"


#define KETL_SIZE_8B    1
#define KETL_SIZE_16B   2
#define KETL_SIZE_32B   4
#define KETL_SIZE_64B   8

#define KETL_REG_AX     0x00
#define KETL_REG_CX     0x01
#define KETL_REG_DX     0x02
#define KETL_REG_BX     0x03
#define KETL_REG_SP     0x04
#define KETL_REG_BP     0x05
#define KETL_REG_SI     0x06
#define KETL_REG_DI     0x07
#define KETL_REG_R8     0x08
#define KETL_REG_R9     0x09
#define KETL_REG_R10    0x0a
#define KETL_REG_R11    0x0b
#define KETL_REG_R12    0x0c
#define KETL_REG_R13    0x0d
#define KETL_REG_R14    0x0e
#define KETL_REG_R15    0x0f

#define KETL_ARG_REG            0x00
#define KETL_ARG_REG_MEM        0x01
#define KETL_ARG_RSP_MEM_DISP   0x02
#define KETL_ARG_RBP_MEM_DISP   0x03
#define KETL_ARG_IMM            0x04
#define KETL_ARG_IMM_MEM        0x05

#define KETL_OP_MOV     0

KETL_DEFINE(x86_op_struct) {
    union {
        struct {
            uint8_t firstArgType;
            uint8_t secondArgType;
        };
        uint16_t argTypesHash;
    };
    uint8_t size;
    uint16_t opCode;
    uint64_t firstArg;
    uint64_t secondArg;
};

#define REX_PREFIX 0x04 // 0b0100

KETL_DEFINE(REXByte) {
    uint8_t b : 1;
    uint8_t x : 1;
    uint8_t r : 1;
    uint8_t w : 1;
    uint8_t prefix : 4;
};

KETL_DEFINE(MODRMByte) {
    uint8_t rm : 3;
    uint8_t reg : 3;
    uint8_t mod : 2;
};

#define MODRM_MOD_IND       0x00 // 0b00
#define MODRM_MOD_IND_DIS8  0x01 // 0b01
#define MODRM_MOD_IND_DIS32 0x02 // 0b10
#define MODRM_MOD_DIR       0x03 // 0b11

KETL_DEFINE(SIBByte) {
    uint8_t base : 3;
    uint8_t index : 3;
    uint8_t scale : 2;
};

#define SIB_SCALE_1 0x00 // 0b00
#define SIB_SCALE_2 0x01 // 0b01
#define SIB_SCALE_4 0x02 // 0b10
#define SIB_SCALE_8 0x03 // 0b11

KETL_VECTOR_DECLARATION(opcodes_t, uint8_t)

#define PUSH_OPCODE_REG_IMM(pOpcodes, sizeArg, opcodeArg, regArg, immArg) do {\
    x86_op_struct __tmpOpStruct;\
    __tmpOpStruct.size = (sizeArg);\
    __tmpOpStruct.firstArgType = KETL_ARG_REG;\
    __tmpOpStruct.secondArgType = KETL_ARG_IMM;\
    __tmpOpStruct.opCode = (opcodeArg);\
    __tmpOpStruct.firstArg = (regArg);\
    __tmpOpStruct.secondArg = (immArg);\
    push_opcode((pOpcodes), &__tmpOpStruct);\
} while(false)

#define PUSH_OPCODE_REG_RSP_DISP(pOpcodes, sizeArg, opcodeArg, regArg, immArg) do {\
    x86_op_struct __tmpOpStruct;\
    __tmpOpStruct.size = (sizeArg);\
    __tmpOpStruct.firstArgType = KETL_ARG_REG;\
    __tmpOpStruct.secondArgType = KETL_ARG_RSP_MEM_DISP;\
    __tmpOpStruct.opCode = (opcodeArg);\
    __tmpOpStruct.firstArg = (regArg);\
    __tmpOpStruct.secondArg = (immArg);\
    push_opcode((pOpcodes), &__tmpOpStruct);\
} while(false)

#define PUSH_OPCODE_REG_RBP_DISP(pOpcodes, sizeArg, opcodeArg, regArg, immArg) do {\
    x86_op_struct __tmpOpStruct;\
    __tmpOpStruct.size = (sizeArg);\
    __tmpOpStruct.firstArgType = KETL_ARG_REG;\
    __tmpOpStruct.secondArgType = KETL_ARG_RBP_MEM_DISP;\
    __tmpOpStruct.opCode = (opcodeArg);\
    __tmpOpStruct.firstArg = (regArg);\
    __tmpOpStruct.secondArg = (immArg);\
    push_opcode((pOpcodes), &__tmpOpStruct);\
} while(false)

#define PUSH_OPCODE_REG_IMM_MEM(pOpcodes, sizeArg, opcodeArg, regArg, immArg) do {\
    x86_op_struct __tmpOpStruct;\
    __tmpOpStruct.size = (sizeArg);\
    __tmpOpStruct.firstArgType = KETL_ARG_REG;\
    __tmpOpStruct.secondArgType = KETL_ARG_IMM_MEM;\
    __tmpOpStruct.opCode = (opcodeArg);\
    __tmpOpStruct.firstArg = (regArg);\
    __tmpOpStruct.secondArg = (immArg);\
    push_opcode((pOpcodes), &__tmpOpStruct);\
} while(false)

#define PUSH_OPCODE_RSP_DISP_REG(pOpcodes, sizeArg, opcodeArg, immArg, regArg) do {\
    x86_op_struct __tmpOpStruct;\
    __tmpOpStruct.size = (sizeArg);\
    __tmpOpStruct.firstArgType = KETL_ARG_RSP_MEM_DISP;\
    __tmpOpStruct.secondArgType = KETL_ARG_REG;\
    __tmpOpStruct.opCode = (opcodeArg);\
    __tmpOpStruct.firstArg = (immArg);\
    __tmpOpStruct.secondArg = (regArg);\
    push_opcode((pOpcodes), &__tmpOpStruct);\
} while(false)

#define PUSH_OPCODE_RBP_DISP_REG(pOpcodes, sizeArg, opcodeArg, immArg, regArg) do {\
    x86_op_struct __tmpOpStruct;\
    __tmpOpStruct.size = (sizeArg);\
    __tmpOpStruct.firstArgType = KETL_ARG_RBP_MEM_DISP;\
    __tmpOpStruct.secondArgType = KETL_ARG_REG;\
    __tmpOpStruct.opCode = (opcodeArg);\
    __tmpOpStruct.firstArg = (immArg);\
    __tmpOpStruct.secondArg = (regArg);\
    push_opcode((pOpcodes), &__tmpOpStruct);\
} while(false)

#define PUSH_OPCODE_IMM_MEM_REG(pOpcodes, sizeArg, opcodeArg, immArg, regArg) do {\
    x86_op_struct __tmpOpStruct;\
    __tmpOpStruct.size = (sizeArg);\
    __tmpOpStruct.firstArgType = KETL_ARG_IMM_MEM;\
    __tmpOpStruct.secondArgType = KETL_ARG_REG;\
    __tmpOpStruct.opCode = (opcodeArg);\
    __tmpOpStruct.firstArg = (immArg);\
    __tmpOpStruct.secondArg = (regArg);\
    push_opcode((pOpcodes), &__tmpOpStruct);\
} while(false)

void push_opcode(opcodes_t* pOpcodes, x86_op_struct* pOpStruct);

#endif // ketl_compiler_assembler_x86_h
