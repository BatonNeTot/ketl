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
};

#endif // ketl_compiler_token_h
