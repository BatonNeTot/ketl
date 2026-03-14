//🫖ketl
#ifndef ketl_error_h
#define ketl_error_h

#include "ketl/utils.h"

#include "atomic_strings.h"

ANN_FORWARD(ketl_state);
ANN_FORWARD(ketl_lexer_t);

ANN_DEFINE(ketl_error_info) {
    ketl_lexer_t* p_lexer;
    ketl_atomic_string s_filename;
    uint32_t offset;
    uint32_t length;
};

void ketl_error_report(ketl_state* p_state, ketl_error_info* p_error_info, const char* format, ...);

#endif
