//🫖ketl
#ifndef ketl_compiler_assembler_x86_h
#define ketl_compiler_assembler_x86_h

#include "ketl/utils.h"


#define REX_PREFIX 0x04 // 0b0100

ANN_DEFINE(REXByte) {
    uint8_t b      : 1; // 1-bit value is an extension to the MODRM.rm field or the SIB.base field
    uint8_t x      : 1; // 1-bit value is an extension to the SIB.index field
    uint8_t r      : 1; // 1-bit value is an extension to the MODRM.reg field
    uint8_t w      : 1; // When 1, a 64-bit operand size is used
    uint8_t prefix : 4; // Magic bytes only
};

ANN_DEFINE(MODRMByte) {
    uint8_t rm : 3;
    uint8_t reg : 3;
    uint8_t mod : 2;
};

#define MODRM_MOD_IND       0x00 // 0b00
#define MODRM_MOD_IND_DIS8  0x01 // 0b01
#define MODRM_MOD_IND_DIS32 0x02 // 0b10
#define MODRM_MOD_DIR       0x03 // 0b11

ANN_DEFINE(SIBByte) {
    uint8_t base : 3;
    uint8_t index : 3;
    uint8_t scale_power : 2;
};

#define SIB_SCALE_1 0x00 // 0b00
#define SIB_SCALE_2 0x01 // 0b01
#define SIB_SCALE_4 0x02 // 0b10
#define SIB_SCALE_8 0x03 // 0b11

uint32_t ketl_asm_x86_format_directive_size(uint8_t size, char* p_buffer, uint32_t buffer_size);

#endif
