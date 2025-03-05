//🫖ketl
#ifndef ketl_bytecode_h
#define ketl_bytecode_h

#include "ketl/utils.h"

typedef uint8_t ketl_bytecode_instr;

enum __KETL_BYTECODE_INSTRUCTION {
    KETL_BYTECODE_STACK_PROLOG,

    KETL_BYTECODE_RETURN,
    KETL_BYTECODE_RETURN_8VALUE,
    KETL_BYTECODE_RETURN_16VALUE,
    KETL_BYTECODE_RETURN_32VALUE,
    KETL_BYTECODE_RETURN_64VALUE,

    KETL_BYTECODE_U8LOAD_CONST,
    KETL_BYTECODE_U16LOAD_CONST,
    KETL_BYTECODE_U32LOAD_CONST,
    KETL_BYTECODE_U64LOAD_CONST,
    KETL_BYTECODE_I8LOAD_CONST,
    KETL_BYTECODE_I16LOAD_CONST,
    KETL_BYTECODE_I32LOAD_CONST,
    KETL_BYTECODE_I64LOAD_CONST,

    KETL_BYTECODE_U8ADD,
    KETL_BYTECODE_U16ADD,
    KETL_BYTECODE_U32ADD,
    KETL_BYTECODE_U64ADD,
    KETL_BYTECODE_I8ADD,
    KETL_BYTECODE_I16ADD,
    KETL_BYTECODE_I32ADD,
    KETL_BYTECODE_I64ADD,

    KETL_BYTECODE_U8MULTIPLY,
    KETL_BYTECODE_U16MULTIPLY,
    KETL_BYTECODE_U32MULTIPLY,
    KETL_BYTECODE_U64MULTIPLY,
    KETL_BYTECODE_I8MULTIPLY,
    KETL_BYTECODE_I16MULTIPLY,
    KETL_BYTECODE_I32MULTIPLY,
    KETL_BYTECODE_I64MULTIPLY,
};

typedef uint16_t ketl_bytecode_stack_offset;

KETL_DEFINE(ketl_bytecode) {
    uint8_t* pInstructions;
    uint8_t* pLabels;
    uint32_t instructionsCount;
};

uint8_t ketl_bytecode_decode_instruction_length(ketl_bytecode_instr instruction);

uint32_t ketl_bytecode_format(uint8_t* pInstruction, uint8_t* pLabels, char* buffer, uint32_t bufferSize);

#endif // ketl_bytecode_h
