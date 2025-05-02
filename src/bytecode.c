//🫖ketl
#include "bytecode.h"

#include <stdio.h>


uint8_t ketl_bytecode_decode_instruction_length(ketl_bytecode_instr instruction) {
    if (instruction <= KETL_BYTECODE_RETURN) {
        if (instruction == KETL_BYTECODE_CALL) {
            return sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset);
        }
        return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset);
    } else if (instruction <= KETL_BYTECODE_64RETURN) {
        return sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset);
    } else if (instruction >= KETL_BYTECODE_8UADD) {
        return sizeof(ketl_bytecode_instr) + 3 * sizeof(ketl_bytecode_stack_offset);
    } else {
        switch (instruction) {
            case KETL_BYTECODE_RETURN:
            return sizeof(ketl_bytecode_instr);
            case KETL_BYTECODE_8LOAD_UCONST:
            case KETL_BYTECODE_8LOAD_ICONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint8_t);
            case KETL_BYTECODE_16LOAD_UCONST:
            case KETL_BYTECODE_16LOAD_ICONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint16_t);
            case KETL_BYTECODE_32LOAD_UCONST:
            case KETL_BYTECODE_32LOAD_ICONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint32_t);
            case KETL_BYTECODE_64LOAD_UCONST:
            case KETL_BYTECODE_64LOAD_ICONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint64_t);
            default:
            return 0;
        }
    }
}

uint32_t ketl_bytecode_format(uint8_t* pInstruction, uint8_t* pLabels, char* buffer, uint32_t bufferSize) {
    (void)pLabels;
    switch(pInstruction[0]) {
        case KETL_BYTECODE_STACK_PROLOG: {
            return snprintf(buffer, bufferSize, "PROLOG %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_8PUSH_ARG:
        case KETL_BYTECODE_16PUSH_ARG:
        case KETL_BYTECODE_32PUSH_ARG:
        case KETL_BYTECODE_64PUSH_ARG: {
            return snprintf(buffer, bufferSize, "PUSH ARG %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_CALL: {
            return snprintf(buffer, bufferSize, "CALL %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_RETURN: {
            return snprintf(buffer, bufferSize, "EPILOG %d, RETURN", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_8RETURN:
        case KETL_BYTECODE_16RETURN:
        case KETL_BYTECODE_32RETURN:
        case KETL_BYTECODE_64RETURN: {
            return snprintf(buffer, bufferSize, "EPILOG %d, RETURN_VALUE, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8LOAD_UCONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint8_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_16LOAD_UCONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint16_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_32LOAD_UCONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint32_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_64LOAD_UCONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %d, %lld", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint64_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8LOAD_ICONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(int8_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_16LOAD_ICONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(int16_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_32LOAD_ICONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(int32_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_64LOAD_ICONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %d, %lld", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(int64_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8UADD:
        case KETL_BYTECODE_16UADD:
        case KETL_BYTECODE_32UADD:
        case KETL_BYTECODE_64UADD: {
            return snprintf(buffer, bufferSize, "UADD %d, %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8IADD:
        case KETL_BYTECODE_16IADD:
        case KETL_BYTECODE_32IADD:
        case KETL_BYTECODE_64IADD: {
            return snprintf(buffer, bufferSize, "ADD %d, %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8UMULTIPLY:
        case KETL_BYTECODE_16UMULTIPLY:
        case KETL_BYTECODE_32UMULTIPLY:
        case KETL_BYTECODE_64UMULTIPLY: {
            return snprintf(buffer, bufferSize, "UMULTIPLY %d, %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8IMULTIPLY:
        case KETL_BYTECODE_16IMULTIPLY:
        case KETL_BYTECODE_32IMULTIPLY:
        case KETL_BYTECODE_64IMULTIPLY: {
            return snprintf(buffer, bufferSize, "MULTIPLY %d, %d, %d", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        default: {
            return snprintf(buffer, bufferSize, "can't format bytecode %d", pInstruction[0]);
        }
    }
}
