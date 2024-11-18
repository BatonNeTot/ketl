//🫖ketl
#ifndef ketl_compiler_lexer_h
#define ketl_compiler_lexer_h

#include "ketl/utils.h"
#include "token.h"


#define KETL_LEXER_INITIAL_TOKEN_CAPACITY 4

const ketl_token* ketl_lexer_build_tokens(const char* pSource, uint32_t length, uint32_t* pCount);

#endif // ketl_compiler_lexer_h
