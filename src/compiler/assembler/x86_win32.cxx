//🫖ketl
#include "compiler/assembler.h"

#include "x86.h"
 
#define PUSH_MOV_STACK_TO_REG(pOpcodes, sizeMacro, regMacro, stackOffset)\
    PUSH_OPCODE_REG_RSP_DISP(pOpcodes, sizeMacro, KETL_OP_MOV, regMacro, stackOffset);

#define PUSH_MOV_REG_TO_STACK(pOpcodes, sizeMacro, stackOffset, regMacro)\
    PUSH_OPCODE_RSP_DISP_REG(pOpcodes, sizeMacro, KETL_OP_MOV, stackOffset, regMacro);

static uint8_t aParameterRegs[] = {
    KETL_REG_CX,
    KETL_REG_DX,
    KETL_REG_R8,
    KETL_REG_R9,
};

static uint32_t countPushArgs(ketl_bytecode bytecode, uint32_t initialIndex) {
    uint32_t remainingPushArgCounter = 0;
    for (uint32_t i = initialIndex; i < bytecode.instructionsCount; 
            i += ketl_bytecode_decode_instruction_length(bytecode.pInstructions[i])) {
        switch(bytecode.pInstructions[i]) {
            case KETL_BYTECODE_64LOAD_CONST: {
                // Do nothing - if function is global var it will be loaded here
                // just skip
                break;
            }
            case KETL_BYTECODE_8PUSH_ARG:
            case KETL_BYTECODE_16PUSH_ARG:
            case KETL_BYTECODE_32PUSH_ARG:
            case KETL_BYTECODE_64PUSH_ARG: {
                ++remainingPushArgCounter;
                break;
            }
            case KETL_BYTECODE_CALL:
                return remainingPushArgCounter;
        }
    }
    // TODO ERROR
    printf("sudden end of bytecode - call instruction expected");
    KETL_ASSERT(false);
}

#define IMM_ARG(type, offset) (*(type*)(bytecode.pInstructions + i + sizeof(ketl_bytecode_instr) + (offset) * sizeof(ketl_bytecode_stack_offset)))

#define PREALLOCATION_CALL_REG_PARAMS_SPACE (8 * (sizeof(aParameterRegs) / sizeof(*aParameterRegs)))
#define SHADOW_STACK_SPACE 8 // I'm not sure about it, I came to this during msvc disasm study

#define NEED_STACK_ALLOC(stackUsage, hasCalls) (hasCalls || stackUsage > 0)

#define ADAPT_STACK_USAGE(stackUsage, hasCalls)\
do {\
    if (hasCalls) {\
        stackUsage += PREALLOCATION_CALL_REG_PARAMS_SPACE;\
    }\
    stackUsage = KETL_ALIGN_FORWARD(stackUsage, 16); /* 16 bites aligned */\
    stackUsage += SHADOW_STACK_SPACE; /* add shadow space AFTER alignment */\
} while(false)

uint8_t* ketl_assembler_compile(ketl_bytecode bytecode, uint32_t* pOpcodesSize, const ketl_allocator* pAllocator) {
    opcodes opcodes;
    opcodes_init(&opcodes, bytecode.instructionsCount, pAllocator);

    bool hasCalls = false;

    for (uint32_t i = 0u; i < bytecode.instructionsCount; 
        i += ketl_bytecode_decode_instruction_length(bytecode.pInstructions[i])) {
        if (bytecode.pInstructions[i == KETL_BYTECODE_CALL]) {
            hasCalls = true;
            break;
        }
    }

    uint32_t remainingPushArgCount = 0;

    for (uint32_t i = 0u; i < bytecode.instructionsCount; 
            i += ketl_bytecode_decode_instruction_length(bytecode.pInstructions[i])) {
        switch (bytecode.pInstructions[i]) {
            case KETL_BYTECODE_STACK_PROLOG: {
                ketl_bytecode_stack_offset stackUsage = IMM_ARG(ketl_bytecode_stack_offset, 0);
                
                if (NEED_STACK_ALLOC(stackUsage, hasCalls)) {
                    uint8_t opcodesArray[] =
                    {
                        0x48, 0x81, 0xec, 0xff, 0xff, 0x00, 0x00,             // sub     rsp, 65535
                    };
                    ADAPT_STACK_USAGE(stackUsage, hasCalls);
                    *(int32_t*)(opcodesArray + 3) = (int32_t)stackUsage;
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }
                break;
            }
            case KETL_BYTECODE_64PUSH_ARG: {
                if (remainingPushArgCount == 0) {
                    remainingPushArgCount = countPushArgs(bytecode, i);
                }
                if (remainingPushArgCount >= sizeof(aParameterRegs) / sizeof(*aParameterRegs)) {
                    // TODO FIX
                    // push onto actual stack
                    KETL_ASSERT(false);
                }
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, aParameterRegs[--remainingPushArgCount], IMM_ARG(ketl_bytecode_stack_offset, 0));
                break;
            }
            case KETL_BYTECODE_CALL: {
                ketl_bytecode_stack_offset stackOffset = IMM_ARG(ketl_bytecode_stack_offset, 1);
                {
                    const uint8_t opcodesArray[] =
                    {
                        0xff, 0x94, 0x24, 0xff, 0xff, 0x00, 0x00,           // call qword ptr [rsp + 65535]                          
                    };
                    *(int32_t*)(opcodesArray + 3) = (int32_t)stackOffset;
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_64B, IMM_ARG(ketl_bytecode_stack_offset, 0), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_RETURN: {
                ketl_bytecode_stack_offset stackUsage = IMM_ARG(ketl_bytecode_stack_offset, 0);
                if (NEED_STACK_ALLOC(stackUsage, hasCalls)) {
                    const uint8_t opcodesArray[] =
                    {
                        0x48, 0x81, 0xc4, 0xff, 0xff, 0x00, 0x00,             // add     rsp, 65535
                    };
                    ADAPT_STACK_USAGE(stackUsage, hasCalls);
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
            case KETL_BYTECODE_64RETURN: {
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_AX, IMM_ARG(ketl_bytecode_stack_offset, 1));
                
                ketl_bytecode_stack_offset stackUsage = IMM_ARG(ketl_bytecode_stack_offset, 0);
                if (NEED_STACK_ALLOC(stackUsage, hasCalls)) {
                    const uint8_t opcodesArray[] =
                    {
                        0x48, 0x81, 0xc4, 0xff, 0xff, 0x00, 0x00,             // add     rsp, 65535
                    };
                    ADAPT_STACK_USAGE(stackUsage, hasCalls);
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
            case KETL_BYTECODE_64ASSIGN: {
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_AX, IMM_ARG(ketl_bytecode_stack_offset, 1));
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_64B, IMM_ARG(ketl_bytecode_stack_offset, 0), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_8LOAD_CONST: {
                PUSH_OPCODE_REG_IMM(&opcodes, KETL_SIZE_8B, KETL_OP_MOV, KETL_REG_AX, IMM_ARG(uint8_t, 1));
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_8B, IMM_ARG(ketl_bytecode_stack_offset, 0), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_16LOAD_CONST: {
                PUSH_OPCODE_REG_IMM(&opcodes, KETL_SIZE_16B, KETL_OP_MOV, KETL_REG_AX, IMM_ARG(uint16_t, 1));
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_16B, IMM_ARG(ketl_bytecode_stack_offset, 0), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_32LOAD_CONST: {
                PUSH_OPCODE_REG_IMM(&opcodes, KETL_SIZE_32B, KETL_OP_MOV, KETL_REG_AX, IMM_ARG(uint32_t, 1));
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_32B, IMM_ARG(ketl_bytecode_stack_offset, 0), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_64LOAD_CONST: {
                PUSH_OPCODE_REG_IMM(&opcodes, KETL_SIZE_64B, KETL_OP_MOV, KETL_REG_AX, IMM_ARG(uint64_t, 1));
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_64B, IMM_ARG(ketl_bytecode_stack_offset, 0), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_64UADD:
            case KETL_BYTECODE_64IADD: {
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_AX, IMM_ARG(ketl_bytecode_stack_offset, 1));
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_CX, IMM_ARG(ketl_bytecode_stack_offset, 2));
                {
                    const uint8_t opcodesArray[] =
                    {
                        0x48, 0x01, 0xc8,   // add rax, rcx                                   
                    };
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_64B, IMM_ARG(ketl_bytecode_stack_offset, 0), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_64USUB:
            case KETL_BYTECODE_64ISUB: {
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_AX, IMM_ARG(ketl_bytecode_stack_offset, 1));
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_CX, IMM_ARG(ketl_bytecode_stack_offset, 2));
                {
                    const uint8_t opcodesArray[] =
                    {
                        0x48, 0x29, 0xc8,   // sub rax, rcx                                   
                    };
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_64B, IMM_ARG(ketl_bytecode_stack_offset, 0), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_64IMULTIPLY: {
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_AX, IMM_ARG(ketl_bytecode_stack_offset, 1));
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_CX, IMM_ARG(ketl_bytecode_stack_offset, 2));
                {
                    const uint8_t opcodesArray[] =
                    {
                        0x48, 0xf7, 0xe9,   // imul rcx // rdx:rax = rax * rcx                                
                    };
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_64B, IMM_ARG(ketl_bytecode_stack_offset, 0), KETL_REG_AX);
                break;
            }
            case KETL_BYTECODE_64IDIVIDE: {
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_AX, IMM_ARG(ketl_bytecode_stack_offset, 1));
                PUSH_MOV_STACK_TO_REG(&opcodes, KETL_SIZE_64B, KETL_REG_CX, IMM_ARG(ketl_bytecode_stack_offset, 2));
                {
                    const uint8_t opcodesArray[] =
                    {
                        0x48, 0x31, 0xd2,   // xor rdx
                        0x48, 0xf7, 0xf1,   // idiv rcx // rdx:rax / rcx = rax, % rcx = rdx                                
                    };
                    opcodes_push_back_ref_n(&opcodes, opcodesArray, sizeof(opcodesArray) / sizeof(*opcodesArray));
                }
                PUSH_MOV_REG_TO_STACK(&opcodes, KETL_SIZE_64B, IMM_ARG(ketl_bytecode_stack_offset, 0), KETL_REG_AX);
                break;
            }
            default: {
                // TODO ERROR
                char aBuffer[256];
                uint32_t length = ketl_bytecode_format(bytecode.pInstructions + i, bytecode.pLabels, aBuffer, sizeof(aBuffer) / sizeof(*aBuffer));
                printf("unknown bytecode: %.*s\n", length, aBuffer);
                KETL_ASSERT(false);
            }
        }
    }
    
    if (pOpcodesSize != NULL) {
        *pOpcodesSize = opcodes.size;
    }
    return opcodes.pData;
}
