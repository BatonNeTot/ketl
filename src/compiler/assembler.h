//🫖ketl
#ifndef ketl_compiler_assembler_h
#define ketl_compiler_assembler_h

#include "ketl/memory.h"
#include "ketl/utils.h"


ANN_FORWARD(ketl_state);

typedef uint32_t ketl_asm_x86_offset_t;

ANN_FORWARD(ketl_asm_x86_instr_t);

ANN_DEFINE(ketl_asm_x86_t) {
    const ketl_allocator* p_allocator;
    
    ketl_asm_x86_instr_t* p_instrs;
    ketl_asm_x86_offset_t size;
};

void ketl_asm_x86_deinit(ketl_asm_x86_t* p_asm_x86);

uint32_t ketl_asm_x86_format(ketl_state* p_state, ketl_asm_x86_t* p_asm_x86, char* p_buffer, uint32_t buffer_size, bool print_offsets);

uint32_t ketl_asm_format_opcodes(uint8_t* p_opcodes, uint32_t opcodes_size, char* p_buffer, uint32_t buffer_size);

#endif // ketl_compiler_assembler_h
