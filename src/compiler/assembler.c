//🫖ketl
#include "compiler/assembler.h"

#include <stdio.h>

uint32_t ketl_asm_x86_format_opcodes(uint8_t* p_opcodes, uint32_t opcodes_size, char* p_buffer, uint32_t buffer_size) {
    uint32_t printed_count = 0u;
    for (uint64_t i = 0; i < opcodes_size; ++i) {
        if (i != 0 && (i & 15) == 0) {
            printed_count += snprintf(p_buffer + printed_count, buffer_size - printed_count, "\n");
        }
        printed_count += snprintf(p_buffer + printed_count, buffer_size - printed_count, "%.2X ", p_opcodes[i]);
    }
    return printed_count;
}
