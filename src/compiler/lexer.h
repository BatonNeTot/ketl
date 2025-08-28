//🫖ketl
#ifndef ketl_compiler_lexer_h
#define ketl_compiler_lexer_h

#include "compiler/token.h"

#include "containers/vector.h"

#include "ketl/memory.h"
#include "ketl/utils.h"


KETL_VECTOR_DECLARATION(ketl_lexer_tokens_t, ketl_token_t)
KETL_VECTOR_DECLARATION(ketl_lexer_lines_t, uint32_t)

ANN_DEFINE(ketl_lexer_t) {
    const ketl_allocator* p_allocator;
    ketl_lexer_tokens_t v_tokens;
    ketl_lexer_lines_t v_lines;
    const char* p_source;
    uint32_t length;
    uint32_t offset;
};

void ketl_lexer_init(ketl_lexer_t* p_lexer, const ketl_allocator* p_allocator);

void ketl_lexer_deinit(ketl_lexer_t* p_lexer);

void ketl_lexer_build_tokens(ketl_lexer_t* p_lexer, const char* p_source, uint32_t length);

uint32_t ketl_lexer_get_line_offset(ketl_lexer_t* p_lexer, uint32_t line);

uint32_t ketl_lexer_find_line(ketl_lexer_t* p_lexer, uint32_t offset);

#endif // ketl_compiler_lexer_h
