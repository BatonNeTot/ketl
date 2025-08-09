//🫖ketl
#include "bytecode.h"

#include <stdio.h>

#define PRIoffset PRIu16

uint8_t ketl_bytecode_decode_instruction_length(ketl_bytecode_instr instruction) {
    if (instruction >= KETL_BYTECODE_8UADD) {
        return sizeof(ketl_bytecode_instr) + 3 * sizeof(ketl_bytecode_stack_offset);
    } else {
        KETL_SWITCH_STRICT (instruction) {
            case KETL_BYTECODE_STACK_PROLOG:
            case KETL_BYTECODE_8PUSH_ARG:
            case KETL_BYTECODE_16PUSH_ARG:
            case KETL_BYTECODE_32PUSH_ARG:
            case KETL_BYTECODE_64PUSH_ARG:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset);
            case KETL_BYTECODE_CALL:
            return sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset);
            case KETL_BYTECODE_JUMP:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_jump_offset);
            case KETL_BYTECODE_8JUMP_IF:
            case KETL_BYTECODE_16JUMP_IF:
            case KETL_BYTECODE_32JUMP_IF:
            case KETL_BYTECODE_64JUMP_IF:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_jump_offset) + sizeof(ketl_bytecode_stack_offset);
            case KETL_BYTECODE_RETURN:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset);
            case KETL_BYTECODE_8RETURN:
            case KETL_BYTECODE_16RETURN:
            case KETL_BYTECODE_32RETURN:
            case KETL_BYTECODE_64RETURN:
            case KETL_BYTECODE_8ASSIGN:
            case KETL_BYTECODE_16ASSIGN:
            case KETL_BYTECODE_32ASSIGN:
            case KETL_BYTECODE_64ASSIGN:
            return sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset);
            case KETL_BYTECODE_8LOAD_CONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint8_t);
            case KETL_BYTECODE_16LOAD_CONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint16_t);
            case KETL_BYTECODE_32LOAD_CONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint32_t);
            case KETL_BYTECODE_64LOAD_CONST:
            return sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset) + sizeof(uint64_t);
        }
    }
}

uint32_t ketl_bytecode_format(uint8_t* pInstruction, uint8_t* pLabels, char* buffer, uint32_t bufferSize) {
    (void)pLabels;
    switch(pInstruction[0]) {
        case KETL_BYTECODE_STACK_PROLOG: {
            return snprintf(buffer, bufferSize, "PROLOG %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_8PUSH_ARG:
        case KETL_BYTECODE_16PUSH_ARG:
        case KETL_BYTECODE_32PUSH_ARG:
        case KETL_BYTECODE_64PUSH_ARG: {
            return snprintf(buffer, bufferSize, "PUSH ARG %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_CALL: {
            return snprintf(buffer, bufferSize, "CALL INTO %"PRIoffset": %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_JUMP: {
            return snprintf(buffer, bufferSize, "JUMP TO %"PRIoffset, 
            *(ketl_bytecode_jump_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_8JUMP_IF:
        case KETL_BYTECODE_16JUMP_IF:
        case KETL_BYTECODE_32JUMP_IF:
        case KETL_BYTECODE_64JUMP_IF: {
            return snprintf(buffer, bufferSize, "IF %"PRIoffset" JUMP TO %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_jump_offset)), 
            *(ketl_bytecode_jump_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_RETURN: {
            return snprintf(buffer, bufferSize, "EPILOG %"PRIoffset", RETURN", 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)));
        }
        case KETL_BYTECODE_8RETURN:
        case KETL_BYTECODE_16RETURN:
        case KETL_BYTECODE_32RETURN:
        case KETL_BYTECODE_64RETURN: {
            return snprintf(buffer, bufferSize, "EPILOG %"PRIoffset", RETURN_VALUE, %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8ASSIGN:
        case KETL_BYTECODE_16ASSIGN:
        case KETL_BYTECODE_32ASSIGN:
        case KETL_BYTECODE_64ASSIGN: {
            return snprintf(buffer, bufferSize, "ASSIGN INTO %"PRIoffset": %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8LOAD_CONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %"PRIoffset", %"PRId8, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint8_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_16LOAD_CONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %"PRIoffset", %"PRId16, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint16_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_32LOAD_CONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %"PRIoffset", %"PRId32, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint32_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_64LOAD_CONST: {
            return snprintf(buffer, bufferSize, "LOAD_CONST %"PRIoffset", %"PRId64, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(uint64_t*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8UADD:
        case KETL_BYTECODE_16UADD:
        case KETL_BYTECODE_32UADD:
        case KETL_BYTECODE_64UADD: {
            return snprintf(buffer, bufferSize, "UADD INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8IADD:
        case KETL_BYTECODE_16IADD:
        case KETL_BYTECODE_32IADD:
        case KETL_BYTECODE_64IADD: {
            return snprintf(buffer, bufferSize, "ADD INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8USUB:
        case KETL_BYTECODE_16USUB:
        case KETL_BYTECODE_32USUB:
        case KETL_BYTECODE_64USUB: {
            return snprintf(buffer, bufferSize, "USUB INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8ISUB:
        case KETL_BYTECODE_16ISUB:
        case KETL_BYTECODE_32ISUB:
        case KETL_BYTECODE_64ISUB: {
            return snprintf(buffer, bufferSize, "SUB INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8UMULTIPLY:
        case KETL_BYTECODE_16UMULTIPLY:
        case KETL_BYTECODE_32UMULTIPLY:
        case KETL_BYTECODE_64UMULTIPLY: {
            return snprintf(buffer, bufferSize, "UMULTIPLY INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8IMULTIPLY:
        case KETL_BYTECODE_16IMULTIPLY:
        case KETL_BYTECODE_32IMULTIPLY:
        case KETL_BYTECODE_64IMULTIPLY: {
            return snprintf(buffer, bufferSize, "MULTIPLY INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8UDIVIDE:
        case KETL_BYTECODE_16UDIVIDE:
        case KETL_BYTECODE_32UDIVIDE:
        case KETL_BYTECODE_64UDIVIDE: {
            return snprintf(buffer, bufferSize, "UDIVIDE INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8IDIVIDE:
        case KETL_BYTECODE_16IDIVIDE:
        case KETL_BYTECODE_32IDIVIDE:
        case KETL_BYTECODE_64IDIVIDE: {
            return snprintf(buffer, bufferSize, "DIVIDE INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8UMODULO:
        case KETL_BYTECODE_16UMODULO:
        case KETL_BYTECODE_32UMODULO:
        case KETL_BYTECODE_64UMODULO: {
            return snprintf(buffer, bufferSize, "UMODULO INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8IMODULO:
        case KETL_BYTECODE_16IMODULO:
        case KETL_BYTECODE_32IMODULO:
        case KETL_BYTECODE_64IMODULO: {
            return snprintf(buffer, bufferSize, "MODULO INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8UEQUAL:
        case KETL_BYTECODE_16UEQUAL:
        case KETL_BYTECODE_32UEQUAL:
        case KETL_BYTECODE_64UEQUAL: {
            return snprintf(buffer, bufferSize, "UEQUAL INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8IEQUAL:
        case KETL_BYTECODE_16IEQUAL:
        case KETL_BYTECODE_32IEQUAL:
        case KETL_BYTECODE_64IEQUAL: {
            return snprintf(buffer, bufferSize, "EQUAL INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8UNOT_EQUAL:
        case KETL_BYTECODE_16UNOT_EQUAL:
        case KETL_BYTECODE_32UNOT_EQUAL:
        case KETL_BYTECODE_64UNOT_EQUAL: {
            return snprintf(buffer, bufferSize, "UNOT_EQUAL INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8INOT_EQUAL:
        case KETL_BYTECODE_16INOT_EQUAL:
        case KETL_BYTECODE_32INOT_EQUAL:
        case KETL_BYTECODE_64INOT_EQUAL: {
            return snprintf(buffer, bufferSize, "NOT_EQUAL INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8ULESS:
        case KETL_BYTECODE_16ULESS:
        case KETL_BYTECODE_32ULESS:
        case KETL_BYTECODE_64ULESS: {
            return snprintf(buffer, bufferSize, "ULESS INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8ILESS:
        case KETL_BYTECODE_16ILESS:
        case KETL_BYTECODE_32ILESS:
        case KETL_BYTECODE_64ILESS: {
            return snprintf(buffer, bufferSize, "LESS INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8ULESS_OR_EQUAL:
        case KETL_BYTECODE_16ULESS_OR_EQUAL:
        case KETL_BYTECODE_32ULESS_OR_EQUAL:
        case KETL_BYTECODE_64ULESS_OR_EQUAL: {
            return snprintf(buffer, bufferSize, "ULESS_OR_EQUAL INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8ILESS_OR_EQUAL:
        case KETL_BYTECODE_16ILESS_OR_EQUAL:
        case KETL_BYTECODE_32ILESS_OR_EQUAL:
        case KETL_BYTECODE_64ILESS_OR_EQUAL: {
            return snprintf(buffer, bufferSize, "LESS_OR_EQUAL INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8UGREATER:
        case KETL_BYTECODE_16UGREATER:
        case KETL_BYTECODE_32UGREATER:
        case KETL_BYTECODE_64UGREATER: {
            return snprintf(buffer, bufferSize, "UGREATER INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8IGREATER:
        case KETL_BYTECODE_16IGREATER:
        case KETL_BYTECODE_32IGREATER:
        case KETL_BYTECODE_64IGREATER: {
            return snprintf(buffer, bufferSize, "GREATER INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8UGREATER_OR_EQUAL:
        case KETL_BYTECODE_16UGREATER_OR_EQUAL:
        case KETL_BYTECODE_32UGREATER_OR_EQUAL:
        case KETL_BYTECODE_64UGREATER_OR_EQUAL: {
            return snprintf(buffer, bufferSize, "UGREATER_OR_EQUAL INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        case KETL_BYTECODE_8IGREATER_OR_EQUAL:
        case KETL_BYTECODE_16IGREATER_OR_EQUAL:
        case KETL_BYTECODE_32IGREATER_OR_EQUAL:
        case KETL_BYTECODE_64IGREATER_OR_EQUAL: {
            return snprintf(buffer, bufferSize, "GREATER_OR_EQUAL INTO %"PRIoffset": %"PRIoffset", %"PRIoffset, 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)), 
            *(ketl_bytecode_stack_offset*)(pInstruction + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
        }
        default: {
            return snprintf(buffer, bufferSize, "can't format bytecode %"PRIu8, pInstruction[0]);
        }
    }
}
