//🫖ketl
#ifndef ketl_compiler_assembler_x86_h
#define ketl_compiler_assembler_x86_h

#include "containers/vector.h"

#include "ketl/utils.h"


#define KETL_SIZE_8B    1
#define KETL_SIZE_16B   2
#define KETL_SIZE_32B   4
#define KETL_SIZE_64B   8

#define KETL_REG_AX     0
#define KETL_REG_CX     1
#define KETL_REG_DX     2
#define KETL_REG_BX     3
#define KETL_REG_SP     4
#define KETL_REG_BP     5
#define KETL_REG_SI     6
#define KETL_REG_DI     7
#define KETL_REG_R8     8
#define KETL_REG_R9     9
#define KETL_REG_R10    10
#define KETL_REG_R11    11
#define KETL_REG_R12    12
#define KETL_REG_R13    13
#define KETL_REG_R14    14
#define KETL_REG_R15    15

#define KETL_ARG_REG            0
#define KETL_ARG_REG_MEM        1
#define KETL_ARG_RSP_MEM_DISP   2
#define KETL_ARG_RBP_MEM_DISP   3
#define KETL_ARG_IMM            4
#define KETL_ARG_IMM_MEM        5

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

#define REX_PREFIX 0b0100

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

#define MODRM_MOD_IND       0b00
#define MODRM_MOD_IND_DIS8  0b01
#define MODRM_MOD_IND_DIS32 0b10
#define MODRM_MOD_DIR       0b11

KETL_DEFINE(SIBByte) {
    uint8_t base : 3;
    uint8_t index : 3;
    uint8_t scale : 2;
};

#define SIB_SCALE_1 0b00
#define SIB_SCALE_2 0b01
#define SIB_SCALE_4 0b10
#define SIB_SCALE_8 0b11

KETL_NAMED_VECTOR_DECLARATION(opcodes, uint8_t)


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
 
#if KETL_OS_WINDOWS
    #define PUSH_MOV_STACK_TO_REG(pOpcodes, sizeMacro, regMacro, stackOffset)\
        PUSH_OPCODE_REG_RSP_DISP(pOpcodes, sizeMacro, KETL_OP_MOV, regMacro, stackOffset);
#else
    #define PUSH_MOV_STACK_TO_REG(pOpcodes, sizeMacro, regMacro, stackOffset)\
        PUSH_OPCODE_REG_RBP_DISP(pOpcodes, sizeMacro, KETL_OP_MOV, regMacro, stackOffset);
#endif

#if KETL_OS_WINDOWS
    #define PUSH_MOV_REG_TO_STACK(pOpcodes, sizeMacro, stackOffset, regMacro)\
        PUSH_OPCODE_RSP_DISP_REG(pOpcodes, sizeMacro, KETL_OP_MOV, stackOffset, regMacro);
#else
    #define PUSH_MOV_REG_TO_STACK(pOpcodes, sizeMacro, stackOffset, regMacro)\
        PUSH_OPCODE_RBP_DISP_REG(pOpcodes, sizeMacro, KETL_OP_MOV, stackOffset, regMacro);
#endif

void push_opcode(ketl_vector_opcodes* pOpcodes, x86_op_struct* pOpStruct);

#endif // ketl_compiler_assembler_x86_h