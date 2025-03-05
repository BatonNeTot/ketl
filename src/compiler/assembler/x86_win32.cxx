//🫖ketl
#include "compiler/assembler.h"

#include "x86.h"

uint8_t* ketl_assembler_compile(ketl_bytecode bytecode, uint32_t* pOpcodesSize, const ketl_allocator* pAllocator) {
    opcodes opcodes;
    opcodes_init(&opcodes, bytecode.instructionsCount, pAllocator);

    for (uint32_t i = 0u; i < bytecode.instructionsCount; 
            i += ketl_bytecode_decode_instruction_length(bytecode.pInstructions[i])) {
        switch (bytecode.pInstructions[i]) {
            case KETL_BYTECODE_STACK_PROLOG: {
                ketl_bytecode_stack_offset stackUsage = *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr));
                
                if (stackUsage > 0) {
                    uint8_t opcodesArray[] =
                    {
                        0x48, 0x81, 0xec, 0xff, 0xff, 0x00, 0x00,             // sub     rsp, 65535
                    };
                    *(int32_t*)(opcodesArray + 3) = (int32_t)stackUsage;
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }
                break;
            }
            case KETL_BYTECODE_U8LOAD_CONST:
            case KETL_BYTECODE_I8LOAD_CONST: {
                PUSH_OPCODE_REG_IMM(&opcodes, KETL_SIZE_8B, KETL_OP_MOV, KETL_REG_AX, *(uint8_t*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_8B, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr)), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_U16LOAD_CONST:
            case KETL_BYTECODE_I16LOAD_CONST: {
                PUSH_OPCODE_REG_IMM(&opcodes, KETL_SIZE_16B, KETL_OP_MOV, KETL_REG_AX, *(uint8_t*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_16B, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr)), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_U32LOAD_CONST:
            case KETL_BYTECODE_I32LOAD_CONST: {
                PUSH_OPCODE_REG_IMM(&opcodes, KETL_SIZE_32B, KETL_OP_MOV, KETL_REG_AX, *(uint8_t*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_32B, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr)), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_U64LOAD_CONST:
            case KETL_BYTECODE_I64LOAD_CONST: {
                PUSH_OPCODE_REG_IMM(&opcodes, KETL_SIZE_64B, KETL_OP_MOV, KETL_REG_AX, *(uint8_t*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_64B, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr)), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_U64ADD:
            case KETL_BYTECODE_I64ADD: {
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_AX, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_CX, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
                {
                    const uint8_t opcodesArray[] =
                    {
                        0x48, 0x01, 0xc8,   // add rax, rcx                                   
                    };
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_64B, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr)), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_I64MULTIPLY: {
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_AX, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_CX, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr) + 2 * sizeof(ketl_bytecode_stack_offset)));
                {
                    const uint8_t opcodesArray[] =
                    {
                        0x48, 0xf7, 0xe9,   // imul rcx // rdx:rax = rax * rcx                                
                    };
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_64B, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr)), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_RETURN_64VALUE: {
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_AX, *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr) + sizeof(ketl_bytecode_stack_offset)));
                
                ketl_bytecode_stack_offset stackUsage = *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr));
                if (stackUsage > 0) {
                    const uint8_t opcodesArray[] =
                    {
                        0x48, 0x81, 0xc4, 0xff, 0xff, 0x00, 0x00,             // add     rsp, 65535
                    };
                    *(int32_t*)(opcodesArray + 3) = (int32_t)stackUsage;
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }

                {
                    const uint8_t opcodesArray[] =
                    {
                        0xc3                    // ret
                    };
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }
                break;
            }
            case KETL_BYTECODE_RETURN: {
                ketl_bytecode_stack_offset stackUsage = *(ketl_bytecode_stack_offset*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr));
                if (stackUsage > 0) {
                    const uint8_t opcodesArray[] =
                    {
                        0x48, 0x81, 0xc4, 0xff, 0xff, 0x00, 0x00,             // add     rsp, 65535
                    };
                    *(int32_t*)(opcodesArray + 3) = (int32_t)stackUsage;
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }

                const uint8_t opcodesArray[] =
                {
                    0xc3                    // ret
                };
                opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                break;
            }
        }
    }
    
    if (pOpcodesSize != NULL) {
        *pOpcodesSize = opcodes.size;
    }
    return opcodes.pData;
}
