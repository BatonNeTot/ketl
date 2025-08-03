//🫖ketl
#ifndef ketl_compiler_token_h
#define ketl_compiler_token_h

#include "ketl/utils.h"

typedef uint8_t ketl_token_type;

$tokenIds

KETL_DEFINE(ketl_token) {
    ketl_token_type type;
    uint8_t length;
    uint16_t prevOffset;
    uint32_t start_pos_line;
    uint32_t end_pos_line;
    uint16_t start_pos_col;
    uint16_t end_pos_col;
};

#endif // ketl_compiler_token_h
