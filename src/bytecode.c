//🫖ketl
#include "bytecode.h"

#include <stdio.h>


uint8_t ketl_bytecode_decode_instruction_length(ketl_bytecode_instr instruction) {
    if (instruction <= KETL_BYTECODE_RETURN) {
        return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset);
    } else if (instruction <= KETL_BYTECODE_RETURN_64VALUE) {
        return sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset);
    } else if (instruction >= KETL_BYTECODE_U8ADD) {
        return sizeof(ketl_bytecode_instr) + 3 * sizeof(ketl_bytecode_stack_offset);
    } else {
        switch (instruction) {
            case KETL_BYTECODE_RETURN:
            return sizeof(ketl_bytecode_instr);
            case KETL_BYTECODE_U8LOAD_CONST:
            case KETL_BYTECODE_I8LOAD_CONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint8_t);
            case KETL_BYTECODE_U16LOAD_CONST:
            case KETL_BYTECODE_I16LOAD_CONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint16_t);
            case KETL_BYTECODE_U32LOAD_CONST:
            case KETL_BYTECODE_I32LOAD_CONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint32_t);
            case KETL_BYTECODE_U64LOAD_CONST:
            case KETL_BYTECODE_I64LOAD_CONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint64_t);
            default:
            return 0;
        }
    }
}

uint32_t ketl_bytecode_format(uint8_t* pInstruction, uint8_t* pLabels, char* buffer, uint32_t bufferSize) {
    (void)pLabels;
    switch(pInstruction[0]) {
        case KETL_BYTECODE_STACK_RESERVE: {
            return sprintf_s(buffer, bufferSize, "RESERVE %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_RETURN_8VALUE:
        case KETL_BYTECODE_RETURN_16VALUE:
        case KETL_BYTECODE_RETURN_32VALUE:
        case KETL_BYTECODE_RETURN_64VALUE: {
            return sprintf_s(buffer, bufferSize, "RETURN_VALUE %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_RETURN: {
            return sprintf_s(buffer, bufferSize, "RETURN %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_U8LOAD_CONST: {
            return sprintf_s(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint8_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_U16LOAD_CONST: {
            return sprintf_s(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint16_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_U32LOAD_CONST: {
            return sprintf_s(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint32_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_U64LOAD_CONST: {
            return sprintf_s(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint64_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_I8LOAD_CONST: {
            return sprintf_s(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(int8_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_I16LOAD_CONST: {
            return sprintf_s(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(int16_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_I32LOAD_CONST: {
            return sprintf_s(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(int32_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_I64LOAD_CONST: {
            return sprintf_s(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(int64_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_U8ADD:
        case KETL_BYTECODE_U16ADD:
        case KETL_BYTECODE_U32ADD:
        case KETL_BYTECODE_U64ADD: {
            return sprintf_s(buffer, bufferSize, "UADD %d, %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_I8ADD:
        case KETL_BYTECODE_I16ADD:
        case KETL_BYTECODE_I32ADD:
        case KETL_BYTECODE_I64ADD: {
            return sprintf_s(buffer, bufferSize, "ADD %d, %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_U8MULTIPLY:
        case KETL_BYTECODE_U16MULTIPLY:
        case KETL_BYTECODE_U32MULTIPLY:
        case KETL_BYTECODE_U64MULTIPLY: {
            return sprintf_s(buffer, bufferSize, "UMULTIPLY %d, %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_I8MULTIPLY:
        case KETL_BYTECODE_I16MULTIPLY:
        case KETL_BYTECODE_I32MULTIPLY:
        case KETL_BYTECODE_I64MULTIPLY: {
            return sprintf_s(buffer, bufferSize, "MULTIPLY %d, %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        default: {
            return sprintf_s(buffer, bufferSize, "can't format bytecode %d", pInstruction[0]);
        }
    }
}